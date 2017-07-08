/* b04902053 鄭淵仁 */

#include <stdio.h>
#include <ctype.h>		// isdigit
#include <string.h>
#include <unistd.h>		// close
#include <sys/stat.h>	// mkfifo
#include <fcntl.h>		// write, read
#include <sys/select.h>	// select
#include <sys/wait.h>	// wait

typedef struct {
	int pidxn;
	int scors;	// scores
} SCORS;
int main(int argc, char *argv[])
{
	int pid[4];
	while(scanf("%d%d%d%d", pid + 0, pid + 1, pid + 2, pid + 3) != EOF)
	{

	int exit = 1;/*{{{*/
	for(int i = 0; i < 4; ++i)
		if(pid[i] != -1) {
			exit = 0;
			break;
		}
	if(exit)
		return 0;/*}}}*/

	// rff[] = judge123.FIFO	// wff[] = judge123_B.FIFO	/*{{{*/
	char rff[] = "judge";		// read fifo
	strcat(rff, argv[1]);

	char wff[4][32];	// write fifo
	int len = strlen(rff);
	for(int i = 0; i < 4; ++i) {
		strcpy(wff[i], rff);
		wff[i][len] = '_';
		wff[i][len + 1] = 'A' + i;
		wff[i][len + 2] = '\0';
		strcat(wff[i], ".FIFO");
		mkfifo(wff[i], 0600);	// mkfifo wff
	}

	strcat(rff, ".FIFO");
	mkfifo(rff, 0600);			// mkfifo rff	/*}}}*/

	// fork 4 children/*{{{*/
	char pidxs[4][2] = {"A", "B", "C", "D"};
	char rkeys[4][4] = {"23", "35", "58", "813"};
	int rkeyns[4] = {23, 35, 58, 813};		// numbers (int)
	pid_t p[4];		// players by fork
	p[0] = fork();
	for(int i = 1; i < 4; ++i) {
		if(p[i - 1]) {	// parent
			p[i] = fork();
		} else {		// child
			execl("./player", "player", argv[1], pidxs[i - 1], rkeys[i - 1], NULL);
			_exit(0);
		}
	}
	if(p[3] == 0) {		// child
		execl("./player", "player", argv[1], pidxs[3], rkeys[3], NULL);
		_exit(0);
	}/*}}}*/

	// parent
	
	int wfd[4];
	int rfd = open(rff, O_RDONLY);
	for(int i = 0; i < 4; ++i)
		wfd[i] = open(wff[i], O_WRONLY);

	SCORS scors[4];/*{{{*/
	for(int i = 0; i < 4; ++i) {
		scors[i].pidxn = i;
		scors[i].scors = 0;
	}/*}}}*/
	int nchs[4] = {0};			// number choose
	int foreverzero[4] = {0};

	for(int i = 0; i < 20; ++i) {
		// select/*{{{*/
		// no respond 3 seconds -> 0 point forever
		struct timeval timeout = {3, 0};
		// struct timeval timeout = {0, 50000};
		select(4, NULL, NULL, NULL, &timeout);
		int resp[4] = {0};	// has responsed
		/*}}}*/

		// read/*{{{*/
		char buf[1024];
		int rbyte = read(rfd, buf, 1024);
		for(int now = 0; now < rbyte; ++now) {
			for( ; now < rbyte && 'A' > buf[now] && buf[now] < 'D'; ++now) ;
			if(now == rbyte)	break;
			int pidxn = buf[now] - 'A';

			int rkey = 0;			// random_key
			for(now += 2; now < rbyte && isdigit(buf[now]); ++now)
				rkey = rkey * 10 + buf[now] - '0';
			// deal with random_key error
			if(rkey != rkeyns[pidxn])
				foreverzero[pidxn] = 1;

			nchs[pidxn] = buf[now + 1] - '0';
			resp[pidxn] = 1;
		}/*}}}*/

		for(int i = 0; i < 4; ++i)
			if(resp[i] == 0) {
				nchs[i] = 0;
				foreverzero[i] = 1;
			}

		// write/*{{{*/
		char nums[9] = "       \n";
		for(int j = 0; j < 4; ++j)
			nums[2 * j] = '0' + nchs[j];
		for(int j = 0; j < 4; ++j)
			write(wfd[j], nums, 9);
		/*}}}*/
		
		// scores/*{{{*/
		int same[6] = {0};	// {1, 3, 5}
		for(int j = 0; j < 4; ++j)
			same[ nchs[j] ]++;
		for(int j = 0; j < 4; ++j)
			if(same[ nchs[j] ] < 2 && !foreverzero[i])
				scors[j].scors += nchs[j];/*}}}*/
	}

	// sort & printf/*{{{*/
	for(int i = 0; i < 4; ++i)
		for(int j = 0; j < i; ++j)
			if(scors[i].scors < scors[j].scors) {
				SCORS tmp = {scors[i].pidxn, scors[i].scors};
				scors[i].pidxn = scors[j].pidxn, scors[i].scors = scors[j].scors;
				scors[j].pidxn = tmp.pidxn, scors[j].scors = tmp.scors;
			}
	int rank[4] = {1, 2, 3, 4};		// rank[3] must be 4 !!
	for(int i = 2; i >= 0; --i)
		if(scors[i].scors == scors[i + 1].scors) {
			rank[i] = rank[i + 1];
		}

	for(int i = 0; i < 4; ++i)
		for(int j = 0; j < 4; ++j)
			if(scors[j].pidxn == i) {
				printf("%d %d\n", pid[i], rank[j]);
				fflush(stdout);
				break;
			}/*}}}*/
		
		// wait & close & unlink	/*{{{*/
		int sta[4];
		for(int i = 0; i < 4; ++i)
			wait( &(sta[i]) );

		for(int i = 0; i < 4; ++i)
			close(wfd[i]);
		close(rfd);

		for(int i = 0; i < 4; ++i)
			unlink(wff[i]);
		unlink(rff);/*}}}*/
	}

	return 0;
}
