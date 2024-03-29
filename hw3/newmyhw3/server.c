/* b04902053 鄭淵仁 */

// before main/*{{{*/
// includes/*{{{*/
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/wait.h>
#include <sys/select.h>
/*}}}*/
// another includes/*{{{*/
#include <time.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/mman.h>/*}}}*/

// defines/*{{{*/
#define TIMEOUT_SEC 5		// timeout in seconds for wait for a connection 
#define MAXBUFSIZE  1024	// timeout in seconds for wait for a connection 
#define NO_USE      0		// status of a http request
#define ERROR	    -1	
#define READING     1		
#define writing     2		
#define ERR_EXIT(a) { perror(a); exit(1); }/*}}}*/

// struct http_server/*{{{*/
typedef struct {
    char hostname[512];		// hostname
    unsigned short port;	// port to listen
    int listen_fd;		// fd to wait for a new connection
} http_server;/*}}}*/

//struct http_request/*{{{*/
typedef struct {
    int conn_fd;		// fd to talk with client
    int status;			// not used, error, reading (from client)
                                // writing (to client)
    char file[MAXBUFSIZE];	// requested file
    char query[MAXBUFSIZE];	// requested query
    char host[MAXBUFSIZE];	// client host
    char* buf;			// data sent by/to client
    size_t buf_len;		// bytes used by buf
    size_t buf_size; 		// bytes allocated for buf
    size_t buf_idx; 		// offset for reading and writing
	// I add it
	int pin[2], pout[2];	// server read from pin[0], write to  pout[1]
							// child  write to  pin[1], read from pout[0]
	pid_t pid;				// the fork pid
	int first_write;		// first 1, than 0
} http_request;/*}}}*/

static char* logfilenameP;	// log file name


// Forwards/*{{{*/
//
static void init_http_server( http_server *svrP,  unsigned short port );
// initailize a http_request instance, exit for error

static void init_request( http_request* reqP );
// initailize a http_request instance

static void free_request( http_request* reqP );
// free resources used by a http_request instance/*}}}*/

// err codes/*{{{*/
static int read_header_and_file( http_request* reqP, int *errP );
// return 0: success, file is buffered in retP->buf with retP->buf_len bytes
// return -1: error, check error code (*errP)
// return 1: continue to it until return -1 or 0
// error code: 
// 1: client connection error 
// 2: bad request, cannot parse request
// 3: method not implemented 
// 4: illegal filename
// 5: illegal query
// 6: file not found
// 7: file is protected/*}}}*/

static void set_ndelay( int fd );/*{{{*/
// Set NDELAY mode on a socket./*}}}*//*}}}*/

static void add_to_buf( http_request *reqP, char* str, size_t len );

static void write_to_header( http_request* reqP, int err , long int len )/*{{{*/
{

	// crazy strcat	to buf	/*{{{*/
	char buf[MAXBUFSIZE] = "";

	if(err == 4 || err == 5) {/*{{{*/
		strcat(buf, "HTTP/1.1 400 Bad Request\015\012Server: SP TOY\015\012");
	} else if(err == 6 || err == 7 || err == 8) {
		strcat(buf, "HTTP/1.1 404 Not Found\015\012Server: SP TOY\015\012");
	} else {
		strcat(buf, "HTTP/1.1 200 OK\015\012Server: SP TOY\015\012");
	}/*}}}*/

	char tmpbuf[100];

	char timebuf[100];
	time_t now = time( (time_t*) 0 );
	(void) strftime( timebuf, sizeof(timebuf), "%a, %d %b %Y %H:%M:%S GMT", gmtime( &now ) );
	sprintf( tmpbuf, "Date: %s\015\012", timebuf );
	strcat(buf, tmpbuf);

	sprintf( tmpbuf, "Content-Length: %ld\015\012", len );
	strcat(buf, tmpbuf);

	strcat(buf, "Content-Type: text/html; charset=UTF-8");

	sprintf( tmpbuf, "Connection: close\015\012\015\012" );
	strcat(buf, tmpbuf);

	/*}}}*/

	// write buf to conn_fd	/*{{{*/
	long int header_len = strlen(buf);
	fprintf( stderr, "OK: writing header: %ld bytes to request fd %d\n" , header_len, reqP->conn_fd );
	int nwritten;
	while( (nwritten = write( reqP->conn_fd, buf, header_len)) < 0 )	;
	fprintf( stderr, "OK: DONE writing header %d bytes to request fd %d\n", nwritten, reqP->conn_fd );
	/*}}}*/

}/*}}}*/

// signal_func
http_server server;		// http server
http_request* requestP = NULL;// pointer to http requests from client
int conn_fd = -1;		// fd for a new connection with client
int maxfd;                  // size of open file descriptor table

int dead_cgi = 0;

typedef struct {
    char c_time_string[100];
} TimeInfo;

char last_time_record[100];
void time_record(pid_t hispid)/*{{{*/
{
    char file[50];
	// pid_t mypid = getpid();
	sprintf(file, "time_%d", hispid);
	fprintf(stderr, "MMAP's FILE : %s\n", file);

	int fd = open(file, O_RDWR);
	TimeInfo *p_map= (TimeInfo*)mmap(0, sizeof(TimeInfo),  PROT_READ,  MAP_SHARED, fd, 0);
    
	fprintf(stderr, "MMAP : %s\n", p_map->c_time_string);
	strncpy(last_time_record, p_map->c_time_string, strlen(p_map->c_time_string));
	last_time_record[strlen(p_map->c_time_string)] = '\0';

	munmap(p_map, sizeof(TimeInfo));
	unlink(file);

    return;
}/*}}}*/

http_request *sigreqP;
typedef void Sigfunc(int);
void siginfo(int signo)
{
	fprintf( stderr, "OK: CATCH a signo: %d\n", signo);

	// TODO:
	// signal again?

	// make signals html
	char buf[MAXBUFSIZE];
	long int buflen = snprintf( buf, sizeof(buf), "<h1>Info</h1>\015\012" );
	add_to_buf( sigreqP, buf, buflen );

	// dead process/*{{{*/
	buflen = snprintf( buf, sizeof(buf), "<p>%d processes died previously.</p>\015\012", dead_cgi );
	add_to_buf( sigreqP, buf, buflen );/*}}}*/

	// Running processes/*{{{*/
	buflen = snprintf( buf, sizeof(buf), "<p>PIDs of Running Processes:" );
	add_to_buf( sigreqP, buf, buflen );
	for(int j = server.listen_fd + 1; j < maxfd + 1; ++j)
		if(requestP[j].conn_fd == j && j != sigreqP->conn_fd) {
			buflen = snprintf( buf, sizeof(buf), " %d" , requestP[j].pid );
			add_to_buf( sigreqP, buf, buflen );
		}
	buflen = snprintf( buf, sizeof(buf), "</p>\015\012" );
	add_to_buf( sigreqP, buf, buflen );/*}}}*/

	// last exit/*{{{*/
    buflen = snprintf(buf, sizeof(buf), "<p>%s</p>\015\012", last_time_record);
	add_to_buf( sigreqP, buf, buflen );/*}}}*/

	write_to_header( sigreqP, 0, requestP[conn_fd].buf_len );
	// write HTML to conn_fd	/*{{{*/
	fprintf( stderr, "OK: writing (buf %p, idx %d) %d bytes to request fd %d\n"
						, requestP[conn_fd].buf, (int) requestP[conn_fd].buf_idx
						, (int) requestP[conn_fd].buf_len, requestP[conn_fd].conn_fd );
	long int nwritten;
	while( (nwritten = write( requestP[conn_fd].conn_fd, requestP[conn_fd].buf, requestP[conn_fd].buf_len )) < 0) ;
	fprintf( stderr, "OK: DONE all writings %ld bytes on fd %d\n\n", nwritten, requestP[conn_fd].conn_fd );/*}}}*/

	// close requestP[conn_fd]/*{{{*/
fprintf( stderr, "INNNNNNNNNNNNNNNNNNNN\n" );
	int sta;
	// wait(&sta);
	waitpid( requestP[conn_fd].pid, &sta, 0);
	close( requestP[conn_fd].conn_fd );
	free_request( &requestP[conn_fd] );/*}}}*/
fprintf( stderr, "OUTTTTTTTTTTTTTTTTTTT\n" );

	return;
}

int main( int argc, char** argv ) {

	signal(SIGUSR1, siginfo);

	// pre main while	/*{{{*/

    struct sockaddr_in cliaddr; // used by accept()
    int clilen;

    int err;			// used by read_header_and_file()
    int i, ret, nwritten;
    

    // Parse args. 
    if ( argc != 3 ) {
        (void) fprintf( stderr, "usage:  %s port# logfile\n", argv[0] );
        exit( 1 );
    }

    logfilenameP = argv[2];

    // Initialize http server
    init_http_server( &server, (unsigned short) atoi( argv[1] ) );

    maxfd = getdtablesize();
    requestP = ( http_request* ) malloc( sizeof( http_request ) * maxfd );
    if ( requestP == (http_request*) 0 ) {
	fprintf( stderr, "out of memory allocating all http requests\n" );
	exit( 1 );
    }
    for ( i = 0; i < maxfd; i ++ )
        init_request( &requestP[i] );
    requestP[ server.listen_fd ].conn_fd = server.listen_fd;
    requestP[ server.listen_fd ].status = READING;

    fprintf( stderr, "\nstarting on %.80s, port %d, fd %d, maxconn %d, logfile %s...\n", server.hostname, server.port, server.listen_fd, maxfd, logfilenameP );/*}}}*/

    while(1)    // Main loop. 
	{

	// request IO multiplexing	// select	/*{{{*/
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(server.listen_fd, &readfds);
	for(int i = server.listen_fd + 1; i < maxfd; ++i)
		if(requestP[i].conn_fd == i) {
			FD_SET( requestP[i].pin[0] , &readfds );	// pipes
		}
	/*}}}*/

	while( select(maxfd + 1, &readfds, NULL, NULL, NULL) <= 0);

	if( FD_ISSET(server.listen_fd, &readfds) ) {

		// Accept
		clilen = sizeof(cliaddr);
		conn_fd = accept( server.listen_fd, (struct sockaddr *) &cliaddr, (socklen_t *) &clilen );
		if ( conn_fd < 0 ) {/*{{{*/
			// if ( errno == EINTR || errno == EAGAIN ) continue; // try again 
			if ( errno == ENFILE ) {
				(void) fprintf( stderr, "out of file descriptor table ... (maxconn %d)\n", maxfd );
			} else {
				ERR_EXIT( "accept" )
			}
			/*}}}*/
		} else {	// conn_fd >= 0	// handle read

			// initialize reqP	/*{{{*/
			requestP[conn_fd].conn_fd = conn_fd;
			requestP[conn_fd].status = READING;		
			strcpy( requestP[conn_fd].host, inet_ntoa( cliaddr.sin_addr ) );
			set_ndelay( conn_fd );
			/*}}}*/

			// get ret	/*{{{*/
			fprintf( stderr, "OK: GETTING a new request... fd %d from %s\n", conn_fd, requestP[conn_fd].host );
			ret = read_header_and_file( &requestP[conn_fd], &err );
			while(ret > 0) {
				ret = read_header_and_file( &requestP[conn_fd], &err );
			}
			requestP[conn_fd].buf_size = 0;
			/*}}}*/

			char *fn = requestP[conn_fd].file;
			char *qu = &(requestP[conn_fd].query[9]);

			if ( ret < 0 ) {	// error for reading http header or requested file	/*{{{*/

				fprintf( stderr, "ERROR: code %d on fd %d\n", err , requestP[conn_fd].conn_fd);

				// make error HTML
				char buf[MAXBUFSIZE];
				http_request* reqP = &(requestP[conn_fd]);
				long int buflen;
				if(err == 4) {/*{{{*/
					buflen = snprintf( buf, sizeof(buf), "<h1>400 Bad Request</h1>\015\012" );
					add_to_buf( reqP, buf, buflen );
					buflen = snprintf( buf, sizeof(buf), "<p>Illegal filename : \"%s\"</p>\015\012", fn );
					add_to_buf( reqP, buf, buflen );
				} else if(err == 5) {
					buflen = snprintf( buf, sizeof(buf), "<h1>400 Bad Request</h1>\015\012" );
					add_to_buf( reqP, buf, buflen );
					buflen = snprintf( buf, sizeof(buf), "<p>Illegal query : \"%s\"</p>\015\012", qu );
					add_to_buf( reqP, buf, buflen );
				} else if(err == 6) {
					buflen = snprintf( buf, sizeof(buf), "<h1>CGI Program Not Found</h1>\015\012" );
					add_to_buf( reqP, buf, buflen );
					char *fn = reqP->file;
					buflen = snprintf( buf, sizeof(buf), "<p>The requested CGI Program \"%s\" was not found.</p>\015\012", fn );
					add_to_buf( reqP, buf, buflen );
				} else if(err == 8) {
					buflen = snprintf( buf, sizeof(buf), "<h1>404 Not Found</h1>\015\012" );
					add_to_buf( reqP, buf, buflen );
					char *qu = reqP->query + 9;
					buflen = snprintf( buf, sizeof(buf)
							, "<p>The requested file \"%s\" was not found on this server.</p>\015\012" , qu );
					add_to_buf( reqP, buf, buflen );
				}/*}}}*/

				// write error HEADER to conn_fd
				write_to_header( reqP, err, reqP->buf_len );

				// write HTML to conn_fd/*{{{*/
				fprintf( stderr, "OK: writing (buf %p, idx %d) %d bytes to request fd %d\n"
									, requestP[conn_fd].buf, (int) requestP[conn_fd].buf_idx
									, (int) requestP[conn_fd].buf_len, requestP[conn_fd].conn_fd );
				while( (nwritten = write( requestP[conn_fd].conn_fd, requestP[conn_fd].buf, requestP[conn_fd].buf_len )) < 0)	;
				fprintf( stderr, "OK: DONE all writings %d bytes on fd %d\n\n", nwritten, requestP[conn_fd].conn_fd );/*}}}*/

				// close requestP[conn_fd]/*{{{*/
				requestP[conn_fd].status = ERROR;
				close( requestP[conn_fd].conn_fd );
				free_request( &requestP[conn_fd] );/*}}}*/

				/*}}}*/
			} else {	// ret == 0

				if( !strcmp(fn, "info") ) {	// INFO/*{{{*/

					fprintf( stderr, "OK: requiring INFOs on fd %d\n", requestP[conn_fd].conn_fd);

					sigreqP = &(requestP[conn_fd]);
					// fork/*{{{*/
					pid_t pid = fork();
					requestP[conn_fd].pid = pid;
					if(pid == 0) {	// child
						kill(getpid(), SIGUSR1);
						exit(0);
					}	// else		// parent	// signal/*}}}*/
					// signal(SIGUSR1, siginfo);

fprintf( stderr, "OUTOUTOUTTTTTTTTT\n" );

					/*}}}*/
				} else {	// read from file_reader and write to conn_fd/*{{{*/

					// build pipes/*{{{*/
					int pin[2], pout[2];
					pipe(pin);
					pipe(pout);
					for(int j = 0; j < 2; ++j) {
						requestP[conn_fd].pin[j]  = pin[j];
						requestP[conn_fd].pout[j] = pout[j];
					}/*}}}*/
					// fork and exec	/*{{{*/
					pid_t pid = fork();
					requestP[conn_fd].pid = pid;
					if(pid == 0) {	// child
						dup2( requestP[conn_fd].pout[0] , 0);
						dup2( requestP[conn_fd].pin[1]  , 1);
						close( requestP[conn_fd].pout[0] );
						close( requestP[conn_fd].pin[1]  );
						char pth[1028] = "./";
						strncat(pth, fn, strlen(fn));
						execl(pth, fn, NULL);
					}	// else		// parent	/*}}}*/
					// write filename to file_reader	/*{{{*/
					while(write(requestP[conn_fd].pout[1] , qu, strlen(qu)) < 0) 	;
					while(write(requestP[conn_fd].pout[1], "\n", 2) < 0)	;
					/*}}}*/

				}/*}}}*/

			}
		}
	}

	// PIPE_FD updates
	for(int i = server.listen_fd + 1; i < maxfd; ++i)
	if( requestP[i].pin[0] >= 0 && FD_ISSET(requestP[i].pin[0], &readfds) ) {

		// read buf from file_reader & write
		char buf[MAXBUFSIZE];
		ssize_t len = MAXBUFSIZE;
		while( (len = read(requestP[i].pin[0], buf, MAXBUFSIZE)) < 0)	 ; 

			int exit_code = 0;
			if(len < MAXBUFSIZE) {	// close file_reader & get exit_code	/*{{{*/
				int sta;
				// wait(&sta);
				waitpid(requestP[i].pid, &sta, 0);
				time_record(requestP[i].pid);
				exit_code = WEXITSTATUS(sta);
				++dead_cgi;
				for(int j = 0; j < 2; ++j) {
					FD_CLR( requestP[i].pin[j] , &readfds );
					FD_CLR( requestP[i].pout[j], &readfds );
					close( requestP[i].pin[j]  );
					close( requestP[i].pout[j] );
				}
			}/*}}}*/

			if(requestP[i].first_write) {	// write HEADER to conn_fd	/*{{{*/
				requestP[i].first_write = 0;
				if(exit_code == 0) {
					struct stat sb;
					stat( requestP[i].query + 9, &sb );
					write_to_header( &(requestP[i]) , 0, (int64_t) sb.st_size );
				} else {	// exit_code != 0
					int errP = 8;
					write_to_header( &(requestP[i]) , errP, (int64_t) len );
				}
			}/*}}}*/

			// write HTML to conn_fd	/*{{{*/
			fprintf( stderr, "OK: writing %ld bytes to fd %d\n" , len, requestP[i].conn_fd );
			while( (nwritten = write( requestP[i].conn_fd, buf, len )) < 0 )	;
			fprintf( stderr, "OK: DONE writing content %d bytes on fd %d\n", nwritten, requestP[i].conn_fd );/*}}}*/

			if(len < MAXBUFSIZE) {	// close requestP[i]	/*{{{*/
				fprintf( stderr, "OK: DONE ALL writings on fd %d\n\n", requestP[i].conn_fd );
				close( requestP[i].conn_fd );
				free_request( &requestP[i] );
			}/*}}}*/

	}

    }
    free( requestP );
    return 0;
}


// ======================================================================================================/*{{{*/
// You don't need to know how the following codes are working

// another includes/*{{{*/
// #include <time.h>
// #include <fcntl.h>
// #include <ctype.h>
// #include <sys/stat.h>
// #include <sys/mman.h>/*}}}*/

// another vars/*{{{*/
// static void add_to_buf( http_request *reqP, char* str, size_t len );
static void strdecode( char* to, char* from );
static int hexit( char c );
static char* get_request_line( http_request *reqP );
static void* e_malloc( size_t size );
static void* e_realloc( void* optr, size_t size );/*}}}*/

static void init_request( http_request* reqP ) {/*{{{*/
    reqP->conn_fd = -1;
    reqP->status = 0;		// not used
    reqP->file[0] = (char) 0;
    reqP->query[0] = (char) 0;
    reqP->host[0] = (char) 0;
    reqP->buf = NULL;
    reqP->buf_size = 0;
    reqP->buf_len = 0;
    reqP->buf_idx = 0;
	for(int i = 0; i < 2; ++i) {
		reqP->pin[i] = -1;
		reqP->pout[i] = -1;
	}
	reqP->pid = -1;
	reqP->first_write = 1;
}/*}}}*/

static void free_request( http_request* reqP ) {/*{{{*/
    if ( reqP->buf != NULL ) {
	free( reqP->buf );
	reqP->buf = NULL;
    }
    init_request( reqP );
}/*}}}*/


#define ERR_RET( error ) { *errP = error; return -1; }/*{{{*/
// return 0: success, file is buffered in retP->buf with retP->buf_len bytes
// return -1: error, check error code (*errP)
// return 1: read more, continue until return -1 or 0
// error code: 
// 1: client connection error 
// 2: bad request, cannot parse request
// 3: method not implemented 
// 4: illegal filename
// 5: illegal query
// 6: file not found
// 7: file is protected
///*}}}*/
// TODO
static int read_header_and_file( http_request* reqP, int *errP ) {/*{{{*/
    // Request variables/*{{{*/
    char* file = (char *) 0;
    char* path = (char *) 0;
    char* query = (char *) 0;
    char* protocol = (char *) 0;
    char* method_str = (char *) 0;
    int r, fd;
    struct stat sb;
    char timebuf[100];
    int buflen;
    char buf[10000];
    time_t now;
    void *ptr;/*}}}*/

    // Read in request from client
    while (1) {
		r = read( reqP->conn_fd, buf, sizeof(buf) );
		if ( r < 0 && ( errno == EINTR || errno == EAGAIN ) ) return 1;
		if ( r <= 0 ) ERR_RET( 1 )
		add_to_buf( reqP, buf, r );
		if ( strstr( reqP->buf, "\015\012\015\012" ) != (char*) 0 ||
			 strstr( reqP->buf, "\012\012" ) != (char*) 0 ) break;
    }
    // fprintf( stderr, "header: %s\n", reqP->buf );

    // Parse the first line of the request.
    method_str = get_request_line( reqP );
    if ( method_str == (char*) 0 ) ERR_RET( 2 )
    path = strpbrk( method_str, " \t\012\015" );
    if ( path == (char*) 0 ) ERR_RET( 2 )
    *path++ = '\0';
    path += strspn( path, " \t\012\015" );
    protocol = strpbrk( path, " \t\012\015" );
    if ( protocol == (char*) 0 ) ERR_RET( 2 )
    *protocol++ = '\0';
    protocol += strspn( protocol, " \t\012\015" );
    query = strchr( path, '?' );
    if ( query == (char*) 0 )
	query = "";
    else
	*query++ = '\0';

    if ( strcasecmp( method_str, "GET" ) != 0 ) ERR_RET( 3 )
    else {
        strdecode( path, path );
        if ( path[0] != '/' ) ERR_RET( 4 )
	else file = &(path[1]);
    }

    if ( strlen( file ) >= MAXBUFSIZE-1 ) ERR_RET( 4 )
    if ( strlen( query ) >= MAXBUFSIZE-1 ) ERR_RET( 5 )

// I add it
	int slen = strlen(file);
	for(int j = 0; j < slen; ++j)
		if( !isalnum(file[j]) && file[j] != '_' ) {
fprintf( stderr, "HERE!!\n" );
			ERR_RET( 4 );
		}

    strcpy( reqP->file, file );
    strcpy( reqP->query, query );

	if ( query[0] == (char) 0 ) {
		if(!strcmp(file, "info"))
			return 0;

		// for file request, read it in buf
		r = stat( reqP->file, &sb );
		if ( r < 0 ) ERR_RET( 6 )

		fd = open( reqP->file, O_RDONLY );
		if ( fd < 0 ) ERR_RET( 7 )

		ERR_RET( 5 );
	}

	if( (slen = strlen(query)) < 10 )
		ERR_RET( 5 );
	if(strncmp(query, "filename=", 9))
		ERR_RET( 5 );
	char *qu = &( query[9] );
	slen -= 9;
	for(int j = 0; j < slen; ++j)
		if( !isalnum(qu[j]) && qu[j] != '_' ) {
			ERR_RET( 5 );
		}

	r = stat( qu, &sb );
	if ( r < 0 ) ERR_RET( 8 )

	fd = open( qu, O_RDONLY );
	if ( fd < 0 ) ERR_RET( 9 )

/*
	reqP->buf_len = 0;

        buflen = snprintf( buf, sizeof(buf), "HTTP/1.1 200 OK\015\012Server: SP TOY\015\012" );
        add_to_buf( reqP, buf, buflen );
     	now = time( (time_t*) 0 );
        (void) strftime( timebuf, sizeof(timebuf), "%a, %d %b %Y %H:%M:%S GMT", gmtime( &now ) );
        buflen = snprintf( buf, sizeof(buf), "Date: %s\015\012", timebuf );
        add_to_buf( reqP, buf, buflen );
	buflen = snprintf(
	    buf, sizeof(buf), "Content-Length: %ld\015\012", (int64_t) sb.st_size );
        add_to_buf( reqP, buf, buflen );
        buflen = snprintf( buf, sizeof(buf), "Connection: close\015\012\015\012" );
        add_to_buf( reqP, buf, buflen );

	ptr = mmap( 0, (size_t) sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0 );
	if ( ptr == (void*) -1 ) ERR_RET( 8 )
        add_to_buf( reqP, ptr, sb.st_size );
	(void) munmap( ptr, sb.st_size );
	close( fd );
	// printf( "%s\n", reqP->buf );
	// fflush( stdout );
	reqP->buf_idx = 0; // writing from offset 0
	return 0;
    }
    */

    return 0;
}/*}}}*/


static void add_to_buf( http_request *reqP, char* str, size_t len ) { /*{{{*/
    char** bufP = &(reqP->buf);
    size_t* bufsizeP = &(reqP->buf_size);
    size_t* buflenP = &(reqP->buf_len);

    if ( *bufsizeP == 0 ) {
		*bufsizeP = len + 500;
		*buflenP = 0;
		*bufP = (char*) e_malloc( *bufsizeP );
    } else if ( *buflenP + len >= *bufsizeP ) {
		*bufsizeP = *buflenP + len + 500;
		*bufP = (char*) e_realloc( (void*) *bufP, *bufsizeP );
    }
    (void) memmove( &((*bufP)[*buflenP]), str, len );
    *buflenP += len;
    (*bufP)[*buflenP] = '\0';
}/*}}}*/

static char* get_request_line( http_request *reqP ) { /*{{{*/
    int begin;
    char c;

    char *bufP = reqP->buf;
    int buf_len = reqP->buf_len;

    for ( begin = reqP->buf_idx ; reqP->buf_idx < buf_len; ++reqP->buf_idx ) {
	c = bufP[ reqP->buf_idx ];
	if ( c == '\012' || c == '\015' ) {
	    bufP[reqP->buf_idx] = '\0';
	    ++reqP->buf_idx;
	    if ( c == '\015' && reqP->buf_idx < buf_len && 
	        bufP[reqP->buf_idx] == '\012' ) {
		bufP[reqP->buf_idx] = '\0';
		++reqP->buf_idx;
	    }
	    return &(bufP[begin]);
	}
    }
    fprintf( stderr, "http request format error\n" );
    exit( 1 );
}/*}}}*/

static void init_http_server( http_server *svrP, unsigned short port ) {/*{{{*/
    struct sockaddr_in servaddr;
    int tmp;

    gethostname( svrP->hostname, sizeof( svrP->hostname) );
    svrP->port = port;
   
    svrP->listen_fd = socket( AF_INET, SOCK_STREAM, 0 );
    if ( svrP->listen_fd < 0 ) ERR_EXIT( "socket" )

    bzero( &servaddr, sizeof(servaddr) );
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl( INADDR_ANY );
    servaddr.sin_port = htons( port );
    tmp = 1;
    if ( setsockopt( svrP->listen_fd, SOL_SOCKET, SO_REUSEADDR, (void*) &tmp, sizeof(tmp) ) < 0 ) 
	ERR_EXIT ( "setsockopt " )
    if ( bind( svrP->listen_fd, (struct sockaddr *) &servaddr, sizeof(servaddr) ) < 0 ) ERR_EXIT( "bind" )

    if ( listen( svrP->listen_fd, 1024 ) < 0 ) ERR_EXIT( "listen" )
}/*}}}*/

// Set NDELAY mode on a socket./*{{{*/
static void set_ndelay( int fd ) {
    int flags, newflags;

    flags = fcntl( fd, F_GETFL, 0 );
    if ( flags != -1 ) {
	newflags = flags | (int) O_NDELAY; // nonblocking mode
	if ( newflags != flags )
	    (void) fcntl( fd, F_SETFL, newflags );
    }
}   /*}}}*/

static void strdecode( char* to, char* from ) {/*{{{*/
    for ( ; *from != '\0'; ++to, ++from ) {
	if ( from[0] == '%' && isxdigit( from[1] ) && isxdigit( from[2] ) ) {
	    *to = hexit( from[1] ) * 16 + hexit( from[2] );
	    from += 2;
	} else {
	    *to = *from;
        }
    }
    *to = '\0';
}/*}}}*/

static int hexit( char c ) {/*{{{*/
    if ( c >= '0' && c <= '9' )
	return c - '0';
    if ( c >= 'a' && c <= 'f' )
	return c - 'a' + 10;
    if ( c >= 'A' && c <= 'F' )
	return c - 'A' + 10;
    return 0;           // shouldn't happen
}/*}}}*/

static void* e_malloc( size_t size ) {/*{{{*/
    void* ptr;

    ptr = malloc( size );
    if ( ptr == (void*) 0 ) {
	(void) fprintf( stderr, "out of memory\n" );
	exit( 1 );
    }
    return ptr;
}/*}}}*/

static void* e_realloc( void* optr, size_t size ) {/*{{{*/
    void* ptr;

    ptr = realloc( optr, size );
    if ( ptr == (void*) 0 ) {
	(void) fprintf( stderr, "out of memory\n" );
	exit( 1 );
    }
    return ptr;
}/*}}}*//*}}}*/
