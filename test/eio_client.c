/*
 * =====================================================================================
 *
 *       Filename:  epoll_client.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  04/13/2012 10:46:10 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#include <sys/socket.h>   
#include <sys/epoll.h>   
#include <netinet/in.h>   
#include <arpa/inet.h>   
#include <fcntl.h>   
#include <unistd.h>   
#include <stdio.h>   
#include <errno.h>   
#include <stdlib.h>
#include <signal.h>
#include <netinet/tcp.h>
#include <time.h>
#include <pthread.h>

#include <string.h>
#include <strings.h>


#define OUTPUT (0)
#define LONG_CONNECT (0)

#define THREADS_NUM (10)

int PORT = 0;
char IP[64] = {0};

size_t Qps = 0;
int g_still_write=1;

void sig_handler(int sig)
{
	if (sig == SIGPIPE) {
		printf("ok=SIGPIPE write bad socket fd\n");
		g_still_write = 0;
	}
}

void* thread_fun(void* v)
{
	// setup a socket
	int client_socket = socket(AF_INET,SOCK_STREAM,0);
	if (-1 == client_socket) {
		printf("err=create_socket\n");
		return NULL;
	}
	struct sockaddr_in server_addr;
	bzero(&server_addr,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT);
	if(inet_aton(IP, &server_addr.sin_addr)==0) {
		printf("invaild ip\n");
		return NULL;
	}
	
	int tmp = 1;
	if (-1 == setsockopt(client_socket,IPPROTO_TCP,TCP_NODELAY,&tmp,sizeof(tmp)))
		printf("err=set_tcp_no_delay");
	
	//int flag = 1;
	//if (-1 == setsockopt(client_socket,SOL_SOCKET,SO_REUSEADDR,&flag,sizeof(tmp)))
	//		printf("err=set_reuse");

	// set timeout
//	struct timeval timeout = {1,100000}; 
//	setsockopt(client_socket,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof(struct timeval));
//	struct timeval tv_timeout = {1,100000};
//	tv_timeout.tv_sec = 2;
//	tv_timeout.tv_usec = 0;
//	if (setsockopt(client_socket, SOL_SOCKET, SO_SNDTIMEO, &tv_timeout, sizeof(struct timeval)) == 0) {
//		printf("[ok=socket] Socket TimeOut[%lu]\n",tv_timeout.tv_sec);
//	}

	int sndbuf = 0;
	socklen_t optlen = sizeof(sndbuf);
    if (0 == getsockopt(client_socket,SOL_SOCKET,SO_SNDBUF,&sndbuf,&optlen))
		printf("[ok=socket] Socket SendBuf[%d]\n",sndbuf);

	if (-1 == connect(client_socket,(struct sockaddr*)&server_addr,sizeof(server_addr))) {
		printf("[err=connect] [errno=%d] [errmsg=%s]\n",errno,strerror(errno));
		return 0;
	} else {
		printf("[ok=connect] Connected ...\n");
	}

	int auto_input = 1;

	size_t total=0,writes=1000;

#define SIZE 64
	char buffer[SIZE] = "a";
	bzero(buffer,SIZE);

	while(g_still_write && writes) {


		if (auto_input) {
			memset(buffer,'c',16);
			buffer[16] = '\n';
		} else {
			if (!scanf("%s",buffer)) 
				;
		}
		if (0 == strncmp(buffer,"quit",strlen("quit"))) { 
			printf("ok=quit\n");
			exit(-1);
			return NULL;
		}

		// write to server
		int n = write(client_socket,buffer,SIZE);
		//printf("%d", n);
		if (n == -1) {
			printf("write() return -1 , error[%s] , total[%lu]\n",strerror(errno),total);
		} else {
			if (OUTPUT)
				printf("write data [%d] bytes\n",n);
			total += n;
		}

		Qps++;
		int rcv = recv(client_socket,buffer,1024,0);
		if (rcv == 0) {
			if (OUTPUT)
				printf("[warn=socket] peer closed\n");
			exit(-1);
			break;
		} else if (rcv == -1) {
			printf("[warn=socket] [fd=%d] [errno=%d] [errmsg=%s]\n",client_socket,errno,strerror(errno));
			exit(-1);
			break;
		} else {
			if (OUTPUT)
				printf("[ok=read] Read from server ...\n");
			if (OUTPUT)
				printf("[ok=socket] fd[%d] read [%d] data from server\n",client_socket,rcv);
		}

		// sleep 1~5 s
		// unsigned int r = (unsigned int)rand() % 5 + 1;
		// sleep(r);


	}
	return NULL;
}


void* qps_monitor(void* v) 
{
	while(1) {
		printf("Qps:%lu\n",Qps);
		Qps = 0;
		sleep(1);
	}
	return NULL;
}

int main (int argc, char** argv)
{
	signal(SIGPIPE,sig_handler);

	if (argc < 3) {
		fprintf(stderr,"ip address and port number not specified %d\n", argc);
		return -1;
	}

	strcpy(IP,argv[1]);
	PORT = atoi(argv[2]);

	fprintf(stderr, "ip %s:%d\n", IP, PORT);

	srand((unsigned int)time(NULL));

	pthread_t pid[THREADS_NUM + 1] = {0};

	int i=0;
	for (;i!=THREADS_NUM;i++)
		pthread_create(&pid[i],NULL,thread_fun,NULL);

	pthread_create(&pid[i],NULL,qps_monitor,NULL);

	for (int i=0;i!=THREADS_NUM;i++) {
		pthread_join(pid[i],NULL);
		fprintf(stderr, "thread exit! [%ld]\n", (long)pid[i]);
	}

	return 0;
}
