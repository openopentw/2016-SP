/*b04902068吳孟軒*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

int main(int argc, char **argv){// ./player [judge_id] [player_index] [random_key]	
	//argv[1] -> judge_id
	//argv[2] -> player_index -> A,B,C,D
	//argv[3] -> random_key	  -> [0,65536)
	// dprintf(2,"jgid %s player_index %s key %s\n",argv[1],argv[2],argv[3]);

	// making filename -> judge[1]_[A].FIFO -> read from judge
	char rfifo[50] = "judge";
	strcat(rfifo,argv[1]);
	strcat(rfifo,"_");
	strcat(rfifo,argv[2]);
	strcat(rfifo,".FIFO");

	// making filename -> judge[1].FIFO -> write to judge
	char wfifo[50] = "judge";
	strcat(wfifo,argv[1]);
	strcat(wfifo,".FIFO");

	// open the fifos
	int fdr = open(rfifo,O_RDONLY);
	int fdw = open(wfifo,O_WRONLY);
	
	// repeating 20 times
	// srand(time(NULL));
	for(int x=0;x<20;x++){
		if(x>0){
			// receive result (except for first time)
			int a,b,c,d;
			char msg[100];
			read(fdr,msg,100);
			sscanf(msg,"%d%d%d%d",&a,&b,&c,&d);
		}
		// making choice
		int choice,flag=0;
		// choice = 2*(rand()%3)+1;
		if(strcmp(argv[2],"A")==0)		{choice = 1;}
		else if(strcmp(argv[2],"B")==0)	{choice = 3;}
		else if(strcmp(argv[2],"C")==0)	{choice = 5;}
		else							{choice = 5;}
		// making response
		char msg[100];
		
		sprintf(msg,"%c%s %d\n",argv[2][0],argv[3],choice);
		// fflush(NULL);
		// if(flag==0)
		write(fdw,msg,100);
	}
	close(fdw);
	close(fdr);
	exit(0);
}