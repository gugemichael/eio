/*
 * =====================================================================================
 *
 *       Filename:  epoll.c
 *
 *    Description:  A example for Linux epoll
 *
 *        Version:  1.0
 *        Created:  03/28/2012 03:40:37 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#include <arpa/inet.h>   
#include <assert.h>
#include <errno.h>   
#include <error.h>
#include <fcntl.h>   
#include <inttypes.h>
#include <netinet/in.h>   
#include <netinet/tcp.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>   
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/epoll.h>
#include <sys/resource.h>
#include <sys/socket.h>   
#include <sys/time.h>
#include <unistd.h>   


#include "eio/eio.h"

#define MAX_CONNS 4096 
#define EPOLL_WAIT_TIMEOUT (-1)

#define EVENT_READABLE 1
#define EVENT_WRITEABLE 2

#define IP_UNKNOWN ("unknown")
#define PORT_UNKNOWN (0)

#define HEART_BEAT_INTERVAL (1)
#define HEART_BEAT_KEEPLIVE (4)

#define BACKLOG 1024

#define SESSION_MONITOR

volatile long qps = 0;

void* qps_monitor(void* a) {
	while(1) {
		fprintf(stderr, "qps is %ld\n", qps);
		qps = 0;
		sleep(1);
	}
}

void tcp_accept(eio_loop* eio, int server_socket, int mask, void* context);
void tcp_write(eio_loop* eio, int server_socket, int mask, void* context);
void tcp_read(eio_loop* eio, int server_socket, int mask, void* context);
void tcp_close(eio_loop* loop, int clientfd);

#if 0
#define nimo_log_info printf
#define nimo_log_debug printf
#define nimo_log_warn printf
#define nimo_log_error printf
#else
#define nimo_log_info(expr,...) do{}while(0)
#define nimo_log_debug(expr,...) do{}while(0)
#define nimo_log_warn(expr,...) do{}while(0)
#define nimo_log_error(expr,...) do{}while(0)
#endif

int LIMIT = 0;

void tcp_error(eio_loop* eio, int server_socket, int mask, void* context) {
	nimo_log_error("error handler on fd %d", server_socket);
	tcp_close(eio, server_socket);
}

void tcp_accept(eio_loop* eio, int server_socket, int mask, void* context)
{
	// the server must be accepted
	struct sockaddr_in client;
	socklen_t client_size = sizeof(struct sockaddr_in);

	int cfd = accept(server_socket,(struct sockaddr*)&client,&client_size);

	nimo_log_info("[accept] fd[%d] connected", cfd);

	if (cfd != -1) {
		int flag = fcntl(cfd,F_GETFL,0);
		// nonblocking
		flag |= O_NONBLOCK;
		if (-1 == fcntl(cfd,F_SETFL,flag))
			nimo_log_error("[warn=socket] socket set O_NONBLOCK faild");
		// no delay (without nagle)
		int nodelay = 1;
		if (-1 == setsockopt(cfd,IPPROTO_TCP,TCP_NODELAY,&nodelay,sizeof(nodelay)))
			nimo_log_error("[warn=socket] socket TCP_NODELAY set faild");

		nimo_log_info("[eio] after accept fd[%d] event %d", cfd, eio_loop_file_event(eio, cfd, EIO_NONE, EIO_EVENT_GET, NULL, NULL));

		// add the client_fd to epoll loop
		eio_loop_file_event(eio, cfd, EIO_READABLE, EIO_EVENT_ADD, tcp_read, NULL);
		nimo_log_info("[eio] fd[%d] event %d", cfd, eio_loop_file_event(eio, cfd, EIO_NONE, EIO_EVENT_GET, NULL, NULL));
		eio_loop_file_event(eio, cfd, EIO_ERR, EIO_EVENT_ADD, tcp_error, NULL);
		nimo_log_info("[eio] fd[%d] event %d", cfd, eio_loop_file_event(eio, cfd, EIO_NONE, EIO_EVENT_GET, NULL, NULL));

	} else {
		nimo_log_error("[err=socket] socket accept faild , errcode : %d, errmsg : %s",errno,strerror(errno));
	}

}


void tcp_close(eio_loop* loop, int clientfd)
{
	nimo_log_info("[ok=socket] Client socket close");

	// clean up event in epoll
	eio_loop_file_event(loop, clientfd, EIO_NONE, EIO_EVENT_CLEAR, NULL, NULL);
	nimo_log_info("[eio] fd[%d] event %d", clientfd, eio_loop_file_event(loop, clientfd, EIO_NONE, EIO_EVENT_GET, NULL, NULL));

	close(clientfd);
}

void tcp_write(eio_loop* eio, int clientfd, int mask, void* context)
{
	qps++;
	//const char* echo = "nimo_eio_server\n";
	const char* echo = "+OK\r\n";
	size_t len = strlen(echo);
	int n = 0;
	while(1) {
		n = write(clientfd,echo,len);
		nimo_log_debug("[ok=write] write [%d] bytes",n);
		if (-1 == n) {
			if (errno != EAGAIN) { 
				nimo_log_error("[error=socket] Write Socket error %s", strerrno(errno));
				break;
			}
			nimo_log_debug("[ok=socket] Write Socket Pause");
		}
		len -= n;
		if (len <= 0)
			break;
	}

	if (-1 != n) {
		eio_loop_file_event(eio, clientfd, EIO_WRITEABLE, EIO_EVENT_DEL, NULL, NULL);
		nimo_log_debug("[eio] fd[%d] event %d", clientfd, eio_loop_file_event(eio, clientfd, EIO_NONE, EIO_EVENT_GET, NULL, NULL));
	} else {
		tcp_close(eio, clientfd);
		nimo_log_error("[eio] write fd[%d] error , closing ");
	}
}

void tcp_read(eio_loop* eio, int clientfd, int mask, void* context)
{
	/**
	 * this method is called by epoll_wait callback if there has 
	 * something to read in buffer
	 */
	int ret = -1;
	char buffer[1024] = {0};
	while(1) {
		ret = read(clientfd,buffer,1024);
		if (LIMIT++ == 1000000)
			;//exit(0);
		if (0 == ret) {
			nimo_log_error("[ok=socket] socket fd[%d] read zero ! closed under mask %d",clientfd, mask);
			tcp_close(eio, clientfd);
			break;
		} else if (-1 == ret) { 
			if (errno == EAGAIN) {
				// would blocking
				eio_loop_file_event(eio, clientfd, EIO_WRITEABLE, EIO_EVENT_ADD, tcp_write, NULL);
				nimo_log_debug("[eio] fd[%d] event %d", clientfd, eio_loop_file_event(eio, clientfd, EIO_NONE, EIO_EVENT_GET, NULL, NULL));
			} else {
				nimo_log_error("[err=socket] read error[%s] , tcp close",strerror(errno));
				tcp_close(eio, clientfd);
			}
			break;
		} else {
			// keep reading 
			nimo_log_debug("[ok=socket] fd[%d] read [%d] bytes",clientfd,ret);
		}
	}
}


#define ut_main main
int ut_main(int argc, char** argv) 
{
	if (argc < 2) {
		fprintf(stderr,"port number not specified %d\n", argc);
		return -1;
	}

	int port = atoi(argv[1]);

	// setup a socket
	int server_socket = socket(AF_INET,SOCK_STREAM,0);
	if (-1 == server_socket) {
		nimo_log_error("[err=socket] socket create faild");
		return -1;
	}
	else
		nimo_log_debug("[ok=socket] socket create success");

	struct sockaddr_in server_addr;
	bzero(&server_addr,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htons(INADDR_ANY);
	server_addr.sin_port = htons(port);

	int flag=1,len=sizeof(flag);

	// we can reuse the port
	setsockopt(server_socket,SOL_SOCKET,SO_REUSEADDR,&flag,len);
	if (-1 == setsockopt(server_socket,IPPROTO_TCP,TCP_NODELAY,&flag,sizeof(flag)))
		nimo_log_error("[warn=socket] socket set SO_REUSEADDR faild");

	// bind ip/port
	if (-1 == bind(server_socket,(struct sockaddr*) &server_addr, sizeof(server_addr))) {
		nimo_log_error("[err=socket] socket bind faild");
		return -1; 
	} else
		nimo_log_debug("[ok=socket] socket bind on port[%d] success",port);
	if (-1 == listen(server_socket,BACKLOG)) {
		nimo_log_error("[err=socket] socket listen faild");
	} else	
		nimo_log_debug("[ok=socket] socket listen backlog[%d] success",BACKLOG);


	// create a epoll server handle
	eio_loop *loop = new_eio_loop(1024);

	loop->hz = 1;

	if (loop == NULL) {
		nimo_log_debug("create eio server error ! ");
		exit(0);
	}

	// firstly listen the server's socket with ACCEPT
	eio_loop_file_event(loop, server_socket, EIO_READABLE, EIO_EVENT_ADD, tcp_accept, NULL);

	printf("eio server run backgroud\n");

	pthread_t pid;
	pthread_create(&pid,NULL,qps_monitor,NULL);

	// do event loop 
	eio_loop_run(loop);

	return 0;
}





