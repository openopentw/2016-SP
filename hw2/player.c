/* b04902053 鄭淵仁 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>	// write, read
#include <fcntl.h>	// open

int main(int argc, char *argv[])
{
	// argv[1] : judge_id		123
	// argv[2] : player_index	B
	int mypid = argv[2][0] - 'A';
	// argv[3] : random_key

	// wff[] = judge123.FIFO	// rff = judge123_B.FIFO
	char wff[] = "judge";		// write fifo
	strcat(wff, argv[1]);
	char rff[32];	// read fifo
	strcpy(rff, wff);
	strcat(wff, ".FIFO");
	strcat(rff, "_");
	strcat(rff, argv[2]);
	strcat(rff, ".FIFO");

	int wfd = open(wff, O_WRONLY);
	int rfd = open(rff, O_RDONLY);
	
	// int nch = atoi(argv[3]) % 3;
	// char ns[3][3] = {"1\n", "3\n", "5\n"};
	// strcat(send, ns[nch]);

	int onum[20][4];	// other's number
	for(int i = 0; i < 20; ++i) {

		char send[32];
		strcpy(send, argv[2]);	// judge_id
		strcat(send, " ");
		strcat(send, argv[3]);	// random_key
		strcat(send, " ");

		// if(i)	sleep(3);
		char nch[2];
		int all5 = 1;
		for(int j = 0; j < 4; ++j)
			if(j != mypid) {
				all5 = 1;
				for(int k = i - 1; k >= 0 && k >= i - 4; --k)
					if(onum[k][j] != 5) {
						all5 = 0;
						break;
					}
				if(all5)	break;
			}
		if(i < 4)
			all5 = 0;
		if(all5) {
			strcpy(nch, "3");
		} else {
			strcpy(nch, "5");
		}
		strcat(send, nch);
		write(wfd, send, strlen(send));
		char buf[32];
		read(rfd, buf, 32);
		for(int j = 0; j < 4; ++j)
			onum[i][j] = buf[2 * j] - '0';
	}

	close(rfd);
	close(wfd);

	return 0;
}
