/* b04902053 鄭淵仁 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <time.h>

#include <sys/types.h>

#define MAXBUFSIZE 1024

typedef struct {
    char c_time_string[100];
} TimeInfo;

void record_time(char *fn) {/*{{{*/
    time_t current_time;
    char c_time_string[100];
    TimeInfo *p_map;
    char file[50] = "time_";
	pid_t mypid = getpid();
	char mypid_c[64];
	sprintf(mypid_c, "%d", mypid);
	strncat(file, mypid_c, strlen(mypid_c));
    
	int fd = open(file, O_RDWR | O_TRUNC | O_CREAT, 0777); 
    if(fd<0)
    {
        perror("open");
        exit(-1);
    }
    lseek(fd,sizeof(TimeInfo),SEEK_SET);
    write(fd,"",1);

    p_map = (TimeInfo*) mmap(0, sizeof(TimeInfo), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    printf("mmap address:%#x\n",(unsigned int)&p_map); // 0x00000
    close(fd);

    current_time = time(NULL);
    strcpy(c_time_string, ctime(&current_time));

    // memcpy(p_map->c_time_string, &c_time_string , sizeof(c_time_string));
	sprintf(p_map->c_time_string, "Last Exit CGI: %s, Filename: %s", c_time_string, fn);
    
    printf("initialize over\n ");

    munmap(p_map, sizeof(TimeInfo));
    printf("umap ok \n");


    fd = open(file, O_RDWR);
    p_map = (TimeInfo*)mmap(0, sizeof(TimeInfo),  PROT_READ,  MAP_SHARED, fd, 0);
    printf("%s\n", p_map->c_time_string);

    return;
}/*}}}*/

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

	record_time(fn);
	close(fd);
	return 0;
}
