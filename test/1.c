#include <unistd.h>/*{{{*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>/*}}}*/
// #include <stdio.h>
// #include <unistd.h>
// #include <fcntl.h>

int main()
{
	struct flock frl = {F_RDLCK, SEEK_SET, 0, 0, 0};
	int fd1 = open("1.c", O_RDONLY);
		printf("%d\n", (frl.l_type == F_RDLCK));
	fcntl(fd1, F_SETLK, &frl);
		printf("%d\n", (frl.l_type == F_RDLCK));
	int fd2 = dup(fd1);
		printf("%d\n", (frl.l_type == F_RDLCK));
	fcntl(fd1, F_GETLK, &frl);
		printf("%d\n", (frl.l_type == F_RDLCK));
	close(fd2);
	close(fd1);
	return 0;
}
