/* b04902053 鄭淵仁 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

typedef struct {
	int pid;
	int scr;	// scores
} SCRS;

// ./big_judge [judge_num] [player_num]
// 1 <= judge_num <= 12
// 4 <= player_num <= 20

// combinationly send messages/*{{{*/
int pn;
char nums[20][3] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20"};
int pids[4];
char pidstr[5000][32];	// 20*19*18*17/4/3/2 = 4845
int npidstr;
int combi(int th, int from)		// 0 -> success	// -1 -> fail
{
	if(th == 4) {
		strcpy(pidstr[ npidstr ], nums[ pids[0] ]);
		for(int i = 1; i < 4; ++i) {
			strcat(pidstr[ npidstr ], " ");
			strcat(pidstr[ npidstr ], nums[ pids[i] ]);
		}
		strcat(pidstr[ npidstr ], "\n");
		++ npidstr;
		return 0;
	}
	if(from + 4 - th > pn)
		return -1;
	int ret = 0;
	for(int i = from; i < pn && ret == 0; ++i) {
		pids[th] = i;
		ret = combi(th + 1, i + 1);
	}
	return 0;
}/*}}}*/

int main(int argc, char *argv[])
{
	int jn = atoi(argv[1]);		// judge_num
	pn = atoi(argv[2]);		// player_num

	// build pipe/*{{{*/
	int pin[jn][2], pout[jn][2];	// read from pin[*][0]	// write to pout[*][1]	// w.r.t. big_judge
	for(int i = 0; i < jn; ++i) {
		pin[i][0] = 3 + i * 2;
		pin[i][1] = 3 + i * 2 + 1;
		pout[i][0] = jn * 2 + 3 + i * 2;
		pout[i][1] = jn * 2 + 3 + i * 2 + 1;
	}

	for(int i = 0; i < jn; ++i) {
		pipe(pin[i]);
		pipe(pout[i]);
	}/*}}}*/

	// fork jn children/*{{{*/
	pid_t p[jn];		// players by fork
	p[0] = fork();
	for(int i = 1; i < jn; ++i) {
		if(p[i - 1]) {	// parent
			p[i] = fork();
		} else {		// child
			dup2(pout[i - 1][0], 0);
			dup2(pin[i - 1][1], 1);
			for(int i = 0; i < jn; ++i) {	// close * 4 * jn	/*{{{*/
				close(pin[i][0]);
				close(pin[i][1]);
				close(pout[i][0]);
				close(pout[i][1]);
			}/*}}}*/
			execl("./judge", "judge", nums[i - 1], NULL);
			_exit(0);
		}
	}
	if(p[jn - 1] == 0) {		// child
		dup2(pout[jn - 1][0], 0);
		dup2(pin[jn - 1][1], 1);
		for(int i = 0; i < jn; ++i) {	// close * 4 * jn	/*{{{*/
			close(pin[i][0]);
			close(pin[i][1]);
			close(pout[i][0]);
			close(pout[i][1]);
		}/*}}}*/
		execl("./judge", "judge", nums[jn - 1], NULL);
		_exit(0);
	}/*}}}*/

	// parent
	// send messages to child/*{{{*/
	npidstr = 0;
	combi(0, 0);

	int Nwrite = npidstr;	// Need to write
	int Nread = 0;		// Need to read

	SCRS scrs[20];/*{{{*/
	for(int i = 0; i < pn; ++i) {
		scrs[i].pid = i + 1;
		scrs[i].scr = 0;
	}/*}}}*/

	// fisrt write some to the pipe/*{{{*/
	int small = (jn < npidstr)? jn: npidstr;
	for(int i = 0; i < small; ++i) {
		write(pout[i][1], pidstr[i], strlen( pidstr[i] ));
		--Nwrite;
		++Nread;
	}/*}}}*/

	while(Nwrite || Nread)
	{
		// select/*{{{*/
		fd_set readfds;
		FD_ZERO(&readfds);
		for(int i = 0; i < jn; ++i)
			FD_SET(pin[i][0], &readfds);
		struct timeval timeout = {5, 0};
		// struct timeval timeout = {1, 0};
		int update = select(jn * 5, &readfds, NULL, NULL, &timeout);/*}}}*/

		if(update > 0)
			for(int i = 0; i < pn && update > 0 ; ++i)
				if( FD_ISSET(pin[i][0], &readfds) ) {
					// read/*{{{*/
					int nread = 4;
					while(nread > 0) {
						char buf[1024];
						int rbyte = read(pin[i][0], buf, 1024);
						for(int now = 0; now < rbyte; ++now) {
							int pid = 0, prank;
							for( ; now < rbyte && !(isdigit(buf[now])) ; ++now) ;
							if(now == rbyte)	break;
							for( ; now < rbyte && isdigit(buf[now]); ++now)
								pid = pid * 10 + buf[now] - '0';
							for( ; now < rbyte && !(isdigit(buf[now])) ; ++now) ;
							prank = buf[now] - '0';
							scrs[pid].scr += 4 - prank;
							--nread;
						}
					}
					--Nread;/*}}}*/

					// write/*{{{*/
					if(Nwrite) {
						write(pout[i][1], pidstr[Nwrite], strlen( pidstr[Nwrite] ));
						--Nwrite;
						++Nread;
					}
					--update;/*}}}*/
				}
	}/*}}}*/

	// sort/*{{{*/
	for(int i = 0; i < pn; ++i)
		for(int j = 0; j < i; ++j)
			if(scrs[i].scr > scrs[j].scr) {
				SCRS tmp = {scrs[i].pid, scrs[i].scr};
				scrs[i].pid = scrs[j].pid, scrs[i].scr = scrs[j].scr;
				scrs[j].pid = tmp.pid, scrs[j].scr = tmp.scr;
			}
	for(int i = 0; i < pn; ++i)
		printf("%d %d\n", scrs[i].pid, scrs[i].scr);/*}}}*/

	// wait & close	/*{{{*/
	for(int i = 0; i < jn; ++i)
		write(pout[i][1], "-1 -1 -1 -1\n", 13);
	int sta[jn];
	for(int i = 0; i < jn; ++i)
		wait( &(sta[i]) );
	for(int i = 0; i < jn; ++i) {	// close * 4 * jn	
		close(pin[i][0]);
		close(pin[i][1]);
		close(pout[i][0]);
		close(pout[i][1]);
	}
	/*}}}*/

	return 0;
}
