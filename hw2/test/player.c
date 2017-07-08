// #include "apue.h"
#include <unistd.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>

int main(int argc,char* argv[]){
	char judge_id[5],player_index[5],random_key[10];
	strcpy(judge_id,argv[1]);
	strcpy(player_index,argv[2]);
	strcpy(random_key,argv[3]);

	char path[1000],w_path[1000];
	strcpy(path,"./judge");
	strcat(path,judge_id);
	strcpy(w_path,path);
	strcat(path,"_");
	strcat(path,player_index);
	strcat(path,".FIFO");
	strcat(w_path,".FIFO");
	// printf("player : %s\n",path);
	// printf("player : %s\n",w_path);
	// fflush(stdout);
	//mkfifo(path,0666);
	int outfd=open(w_path,O_WRONLY);
	int infd=open(path,O_RDONLY);
	dup2(outfd,1);
	dup2(infd,0);
	int max;
	int a,b,c,d;
	for(int i=0;i<20;i++){
		char last_re[4][10];
		char choosen;
		char t[100];
		/*
		if(i==0){
			choosen='3';
			rem=3;
			printf("%s %s %c\n",player_index,random_key,choosen);
		}
		else{
			if(max==1) choosen='1';
			else if(max==3) choosen='3';
			else if(max==5) choosen='5';
			//choosen='5';
			printf("%s %s %c\n",player_index,random_key,choosen);
		}*/
		printf("%s %s %c\n",player_index,random_key,'5');
		fflush(stdout);
		//read(0,t,100);
		//write(1,t,strlen(t));
		//if(i!=0) scanf("%c",&t);
		char buf[100];
		read(0,buf,100);
		sscanf(buf, "%d%d%d%d",&a,&b,&c,&d);
		//printf("%s",buf);
		//scanf("%s%s%s%s",last_re[0],last_re[1],last_re[2],last_re[3]);
		//printf("%s %s %s %s\n",last_re[0],last_re[1],last_re[2],last_re[3]);
		// fflush(stdin);
	}
	//printf("total=%d\n",to);
	return 0;
}
