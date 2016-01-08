
easy-event-io
=============
tcp read/write io event library on epoll and kqueue

Any discuss or issue can be propose in this repository or 
int this blog http://blog.csdn.net/gugemichael

Getting started
----------------
Compile with makefile:
```bash
cd eio ; make 
```

Or compile with gcc directly:
```bash
cd eio 
gcc -std=gnu99 -Wall -Werror -O2 -c -o eio.o eio.c
ar -r libeio.a ./*.o
```

Usage
----------------------

1. Required include header:

```c
#include <eio/eio.h>
```

2. eio sample code:

```c

int main() {

	// create a eio loop server ! max event number 1024 
	eio_loop *loop = new_eio_loop(1024);

	if (!loop) {
		fprintf(stderr, "create eio server error ! ");
		exit(-1);
	}

	// listen the server socket with ACCEPT event
	eio_loop_file_event(loop, server_socket, EIO_READABLE, EIO_EVENT_ADD, tcp_accept, NULL);

	fprintf(stdout, "eio server is running !\n");

	// do event loop 
	eio_loop_run(loop);


	// ............


	// cleanup
	eio_loop_stop(loop);
	eio_loop_destroy(loop);

}

```
