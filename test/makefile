CC = gcc -std=gnu99 -Wall -Werror -pg --coverage -g
PROFILER = #-lprofiler
LIBS = -I../output/ -L../output/ -leio -lpthread ${PROFILER}
RM = rm -rf 

all : 
	${CC} eio_server.c ${LIBS} -o eio_server
	${CC} eio_client.c ${LIBS} -o eio_client

clean :
	${RM} *.core core.* gmon.out logs *.gcno *.gcda eio_client eio_server
