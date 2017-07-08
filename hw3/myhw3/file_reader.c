#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#define MAXBUFSIZE 1024

int main()
{
	char fn[MAXBUFSIZE];
	scanf("%s", fn);	fflush(stdin);

//	printf("<p>The requested file \"%s\" was not found on this server</p>\015\012", fn);	fflush(stdout);
//	exit(-1);

	int fd = open(fn, O_RDONLY);/*{{{*/
	if(fd == -1) {
		// TODO: error code
		printf("<p>The requested file \"%s\" was not found on this server</p>\015\012", fn);	fflush(stdout);
		close(fd);
		exit(-1);
	}/*}}}*/

	char buf[MAXBUFSIZE];
	ssize_t len;
	while( (len = read(fd, buf, MAXBUFSIZE)) > 0 ) {
		// sleep(2);
		write(1, buf, len);
	}

	close(fd);
	return 0;
}
