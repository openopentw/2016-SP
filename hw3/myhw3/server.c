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
#include <ctype.h>
#include <time.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <signal.h>
/*}}}*/

// defines/*{{{*/
#define TIMEOUT_SEC 5		// timeout in seconds for wait for a connection 
#define MAXBUFSIZE  1024	// timeout in seconds for wait for a connection 
#define NO_USE	  0		// status of a http request
#define ERROR		-1	
#define READING	 1		
#define WRITING	 2		
#define ERR_EXIT(a) { perror(a); exit(1); }/*}}}*/

// STRUCT http_server/*{{{*/
typedef struct {
	char hostname[512];		// hostname
	unsigned short port;	// port to listen
	int listen_fd;		// fd to wait for a new connection
} http_server;/*}}}*/

// STRUCT http_request/*{{{*/
typedef struct {
	int conn_fd;		// fd to talk with client
	int status;			// not used, error, reading (from client)
										 // writing (to client)
	char file[MAXBUFSIZE];	// requested file
	char query[MAXBUFSIZE];	// requested query
	char host[MAXBUFSIZE];	// client host
	char* buf;				// data sent by/to client
	size_t buf_len;			// bytes used by buf
	size_t buf_size; 		// bytes allocated for buf
	size_t buf_idx; 		// offset for reading and writing
	// I add it
	int pin[2], pout[2];	// server read from pin[0], write to  pout[1]
							// child  write to  pin[1], read from pout[0]
} http_request;/*}}}*/

static char* logfilenameP;	// log file name


// Forwards func.s	/*{{{*/
//
static void init_http_server( http_server *svrP,  unsigned short port );
// initailize a http_request instance, exit for error

static void init_request( http_request* reqP );
// initailize a http_request instance

static void free_request( http_request* reqP );
// free resources used by a http_request instance/
/*}}}*/

// error codes		/*{{{*/
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
// 7: file is protected
// 8: query not found
///*}}}*/

static void set_ndelay( int fd );
// Set NDELAY mode on a socket./*}}}*/

static void add_to_buf( http_request *reqP, char* str, size_t len );

typedef void Sigfunc(int);
void siginfo(int signo)
{
	fprintf( stderr, "OK: CATCH a signo: %d\n", signo);
	// TODO:
	// show child's running or terminated
	return;
}

int main( int argc, char** argv ) {

	// vars		/*{{{*/

	http_server server;				// http server
	http_request* requestP = NULL;	// pointer to http requests from client

	int maxfd;						// size of open file descriptor table

	struct sockaddr_in cliaddr;		// used by accept()
	int clilen;

	int conn_fd;		// fd for a new connection with client
	int err;			// used by read_header_and_file()
	int i, ret, nwritten;

	/*}}}*/
	

	// Parse args.	// Initialize http server		/*{{{*/
	if ( argc != 3 ) {
		(void) fprintf( stderr, "USAGE:  %s port# logfile\n", argv[0] );
		exit( 1 );
	}

	logfilenameP = argv[2];

	// Initialize http server
	init_http_server( &server, (unsigned short) atoi( argv[1] ) );/*}}}*/

	// initialize requestP	// convert server.listen_fd to requestP		/*{{{*/
	maxfd = getdtablesize();
	requestP = ( http_request* ) malloc( sizeof( http_request ) * maxfd );
	if ( requestP == (http_request*) 0 ) {/*{{{*/
		fprintf( stderr, "OUT OF MEMORY allocating all http requests\n" );
		exit( 1 );
	}/*}}}*/
	for ( i = 0; i < maxfd; i ++ )
		init_request( &requestP[i] );
	requestP[ server.listen_fd ].conn_fd = server.listen_fd;
	requestP[ server.listen_fd ].status = READING;

	fprintf( stderr, "\nSTARTING on %.80s, port %d, fd %d, maxconn %d, logfile %s...\n", 
						server.hostname, server.port, server.listen_fd, maxfd, logfilenameP );	/*}}}*/

	// Main loop. 
	while(1)
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

			// Accept a connection.	/*{{{*/
			clilen = sizeof(cliaddr);
			conn_fd = accept( server.listen_fd, (struct sockaddr *) &cliaddr, (socklen_t *) &clilen );

			if ( conn_fd < 0 ) {/*{{{*/
				// if ( errno == EINTR || errno == EAGAIN ) continue; // try again 
				if ( errno == ENFILE ) {
					(void) fprintf( stderr, "OUT OF FILE DESCRIPTOR table ... (maxconn %d)\n", maxfd );
					// continue;
				} else {
					ERR_EXIT( "accept" )
				}
			}	/*}}}*/
			// conn_fd >= 0		// accept successfully

			// initialize reqP/*{{{*/
			requestP[conn_fd].conn_fd = conn_fd;
			requestP[conn_fd].status = READING;		
			strcpy( requestP[conn_fd].host, inet_ntoa( cliaddr.sin_addr ) );
			set_ndelay( conn_fd );/*}}}*/

			fprintf( stderr, "\nOK: Accept a new request... fd %d from %s\n", conn_fd, requestP[conn_fd].host );/*}}}*/

			// handle read/*{{{*/

			ret = read_header_and_file( &requestP[conn_fd], &err );/*{{{*/
			while ( ret > 0 ) {		// continue to it until return -1 or 0
				ret = read_header_and_file( &requestP[conn_fd], &err );
			}/*}}}*/

			char *fn = requestP[conn_fd].file;
			char *qu = &(requestP[conn_fd].query[9]);

			if ( ret < 0 ) {	// error for reading http header or requested file

				fprintf( stderr, "ERROR: on fd %d, code %d\n" , requestP[conn_fd].conn_fd, err );

				// write error code to header
				// vars	/*{{{*/
				char buf[MAXBUFSIZE] = "";
				char timebuf[100];
				struct stat sb;	/*}}}*/
				// crazy strcat	to buf	/*{{{*/
				char tmpbuf[100];
				if(err == 4 || err == 5) {/*{{{*/
					strcat(buf, "HTTP/1.1 400 Bad Request\015\012Server: SP TOY\015\012");
				} else if(err == 6 || err == 7 || err == 8) {
					strcat(buf, "HTTP/1.1 404 Not Found\015\012Server: SP TOY\015\012");
				} else {
					fprintf( stderr, "BUG!!: there is another condition here !!!\n");
				}/*}}}*/
				time_t now = time( (time_t*) 0 );
				(void) strftime( timebuf, sizeof(timebuf), "%a, %d %b %Y %H:%M:%S GMT", gmtime( &now ) );
				sprintf( tmpbuf, "Date: %s\015\012", timebuf );
				strcat(buf, tmpbuf);
				sprintf( tmpbuf, "Content-Length: %ld\015\012", (int64_t) sb.st_size );
				strcat(buf, tmpbuf);
				strcat(buf, "Content-Type: text/html; charset=UTF-8");
				sprintf( tmpbuf, "Connection: close\015\012\015\012" );
				strcat(buf, tmpbuf);
				if(err == 6) {/*{{{*/
					strcat(buf, "<h1>CGI Program Not Found</h1>\015\012");
					sprintf( tmpbuf, "<p>The requested CGI Program \"%s\" was not found.</p>\015\012", fn );
					strcat(buf, tmpbuf);
				} else if(err == 8) {
					strcat(buf, "<h1>404 Not Found</h1>\015\012");
					sprintf( tmpbuf, "<p>The requested file \"%s\" was not found on this server.</p>\015\012", qu );
					strcat(buf, tmpbuf);
				}/*}}}*/
				/*}}}*/
				// write buf to conn_fd	& close request	/*{{{*/
				int len = strlen(buf);
				fprintf( stderr, "OK: WRITING ERROR: %d bytes to request fd %d\n" , len, requestP[conn_fd].conn_fd );
				nwritten = write( requestP[conn_fd].conn_fd, buf, len);
				fprintf( stderr, "OK: DONE writing ERROR %d bytes to request fd %d\n", nwritten, requestP[conn_fd].conn_fd );

				requestP[conn_fd].status = ERROR;
				close( requestP[conn_fd].conn_fd );
				free_request( &requestP[conn_fd] );/*}}}*/

			} else {	// ret == 0		// success, file is buffered in retP->buf

				// INFO
				if( !strcmp(fn, "info") ) {

					fprintf( stderr, "OK: WRITING INFOs on fd %d\n", requestP[conn_fd].conn_fd);

					// vars	/*{{{*/
					char buf[MAXBUFSIZE] = "";
					char timebuf[100];	/*}}}*/
					// crazy strcat	to buf	/*{{{*/
					char tmpbuf[MAXBUFSIZE];
					strcat(buf, "HTTP/1.1 200 OK\015\012Server: SP TOY\015\012");
					time_t now = time( (time_t*) 0 );
					(void) strftime( timebuf, sizeof(timebuf), "%a, %d %b %Y %H:%M:%S GMT", gmtime( &now ) );
					sprintf( tmpbuf, "Date: %s\015\012", timebuf );
					strcat(buf, tmpbuf);
					strcat(buf, "Content-Type: text/html; charset=UTF-8");
					sprintf( tmpbuf, "Connection: close\015\012\015\012" );
					strcat(buf, tmpbuf);	/*}}}*/
					// write to conn_fd	/*{{{*/
					int len = strlen(buf);
					fprintf( stderr, "OK: WRITING HEADER: %d bytes to request fd %d\n" , len, requestP[conn_fd].conn_fd );
					nwritten = write( requestP[conn_fd].conn_fd, buf, len);
					fprintf( stderr, "OK: DONE writing HEADER %d bytes to request fd %d\n", nwritten, requestP[conn_fd].conn_fd );
					/*}}}*/

					// TODO
					// fork	/*{{{*/
					pid_t pid = fork();
					if(pid == 0) {	// child
						kill(getpid(), SIGUSR1);
						exit(0);
					}	// else		// parent	// signal
					signal(SIGUSR1, siginfo);
					int sta;
					wait(&sta);
					close( requestP[conn_fd].conn_fd );
					free_request( &requestP[conn_fd] );
					/*}}}*/

				} else {

				// build pipes/*{{{*/
				int pin[2], pout[2];
				pipe(pin);
				pipe(pout);
				for(int j = 0; j < 2; ++j) {
					requestP[conn_fd].pin[j]  = pin[j];
					requestP[conn_fd].pout[j] = pout[j];
				}/*}}}*/
				// fork and exec/*{{{*/
				pid_t pid = fork();
				if(pid == 0) {	// child
					dup2( requestP[conn_fd].pout[0] , 0);
					dup2( requestP[conn_fd].pin[1]  , 1);
					close( requestP[conn_fd].pout[0] );
					close( requestP[conn_fd].pin[1]  );
					char pth[1028] = "./";
					strncat(pth, fn, strlen(fn));
					execl(pth, fn, NULL);
					// execl("./file_reader.c", "file_reader", NULL);
				}	// else		// parent	/*}}}*/
				// write filename to file_reader	/*{{{*/
				write(requestP[conn_fd].pout[1] , qu, strlen(qu));
				write(requestP[conn_fd].pout[1], "\n", 2);
				/*}}}*/

				}

			}/*}}}*/

		}

		// PIPE_FD updates
		for(int i = server.listen_fd + 1; i < maxfd; ++i)
		if( requestP[i].pin[0] >= 0 && FD_ISSET(requestP[i].pin[0], &readfds) ) {

			// read buf from file_reader & add to buf	/*{{{*/
			char buf[MAXBUFSIZE];
			ssize_t len = MAXBUFSIZE;
			if( (len = read(requestP[i].pin[0], buf, MAXBUFSIZE)) > 0) {

				add_to_buf( &(requestP[i]) , buf, len);
				fprintf( stderr, "OK: DONE writing content %ld bytes to buf\n", len );

				if(len < MAXBUFSIZE) {

				// close file_reader & get exit_code	/*{{{*/
					int sta;
					wait(&sta);
					int exit_code = WEXITSTATUS(sta);
					for(int j = 0; j < 2; ++j) {
						FD_CLR( requestP[i].pin[j] , &readfds );
						FD_CLR( requestP[i].pout[j], &readfds );
						close( requestP[i].pin[j]  );
						close( requestP[i].pout[j] );
					}/*}}}*/

				// write http header	/*{{{*/
					// vars
					char buf[MAXBUFSIZE] = "";
					char timebuf[100];
					// crazy strcat	to buf	/*{{{*/
					char tmpbuf[MAXBUFSIZE];
					if(exit_code != 0) {
						fprintf( stderr, "ERROR: file_reader exit -1\n" );
						strcat(buf, "HTTP/1.1 404 Not Found\015\012Server: SP TOY\015\012");
					} else {	// no error return
						strcat(buf, "HTTP/1.1 200 OK\015\012Server: SP TOY\015\012");
					}
					time_t now = time( (time_t*) 0 );
					(void) strftime( timebuf, sizeof(timebuf), "%a, %d %b %Y %H:%M:%S GMT", gmtime( &now ) );
					sprintf( tmpbuf, "Date: %s\015\012", timebuf );
					strcat(buf, tmpbuf);
					strcat(buf, "Content-Type: text/html; charset=UTF-8");
					sprintf( tmpbuf, "Connection: close\015\012\015\012" );
					strcat(buf, tmpbuf);
					if(exit_code != 0) {
						strcat(buf, "<h1>404 Not Found</h1>\015\012");
					}	/*}}}*/
					// write to conn_fd	/*{{{*/
					int len = strlen(buf);
					fprintf( stderr, "OK: WRITING HEADER: %d bytes to request fd %d\n" , len, requestP[i].conn_fd );
					nwritten = write( requestP[i].conn_fd, buf, len);
					fprintf( stderr, "OK: DONE writing HEADER %d bytes to request fd %d\n", nwritten, requestP[i].conn_fd );

					requestP[i].status = WRITING;
					/*}}}*/
					/*}}}*/

					fprintf( stderr, "OK: WRITING content %ld bytes to request fd %d\n" , len, requestP[i].conn_fd );
					nwritten = write( requestP[i].conn_fd, buf, len);
					fprintf( stderr, "OK: COMPLETE writing content %d bytes to request fd %d\n", nwritten, requestP[i].conn_fd );

					// close requestP[i]	
					// TODO: sometimes BUG HERE
					if(len >= 0) {
						fprintf( stderr, "OK: DONE all writings on fd %d\n\n", requestP[i].conn_fd );
						requestP[i].status = NO_USE;
					}
					// FD_CLR( requestP[i].conn_fd , &readfds );
					close( requestP[i].conn_fd );
					free_request( &requestP[i] );
				}

			}/*}}}*/

		}

	}

fprintf( stderr, "GO OUT SIDE!!!!!!!!!!!!!!!!\n");

	free( requestP );
	return 0;
}


// ======================================================================================================/*{{{*/
// You don't need to know how the following codes are working

// includes/*{{{*/
// #include <time.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/mman.h>
/*}}}*/

// vars/*{{{*/
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
	reqP->first_write = 1;
}/*}}}*/

static void free_request( http_request* reqP ) {/*{{{*/
	if ( reqP->buf != NULL ) {
	free( reqP->buf );
	reqP->buf = NULL;
	}
	init_request( reqP );
}/*}}}*/


// TODO:
// important comments are here
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
// 8: query not found
// 9: query is protected
///*}}}*/
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

	int slen = strlen(file);
	for(int j = 0; j < slen; ++j)
		if( !isalnum(file[j]) && file[j] != '_' ) {
			ERR_RET( 4 );
		}
	  
	strcpy( reqP->file, file );
	strcpy( reqP->query, query );

// TODO:
///*
	if ( query[0] == (char) 0 ) {
		if(!strcmp(file, "info")) {
			return 0;
		}

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

//	reqP->buf_len = 0;/*{{{*/
//
//		buflen = snprintf( buf, sizeof(buf), "HTTP/1.1 200 OK\015\012Server: SP TOY\015\012" );
//		add_to_buf( reqP, buf, buflen );
//	 	now = time( (time_t*) 0 );
//		(void) strftime( timebuf, sizeof(timebuf), "%a, %d %b %Y %H:%M:%S GMT", gmtime( &now ) );
//		buflen = snprintf( buf, sizeof(buf), "Date: %s\015\012", timebuf );
//		add_to_buf( reqP, buf, buflen );
//	buflen = snprintf(
//		buf, sizeof(buf), "Content-Length: %ld\015\012", (int64_t) sb.st_size );
//		add_to_buf( reqP, buf, buflen );
//		buflen = snprintf( buf, sizeof(buf), "Connection: close\015\012\015\012" );
//		add_to_buf( reqP, buf, buflen );/*}}}*/
/*
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

// Set NDELAY mode on a socket.
static void set_ndelay( int fd ) {/*{{{*/
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
	return 0;		   // shouldn't happen
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
}/*}}}*/
/*}}}*/