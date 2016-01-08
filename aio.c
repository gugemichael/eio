/* 
 * Implemention of nimo eio network library
 *
 * */

#include "eio.h"

#include <linux/version.h>

/** 
 * new_eio_loop 
 */
eio_loop* new_eio_loop(unsigned int max) {

	eio_loop *eio = (eio_loop*) calloc(1, sizeof(eio_loop));

	if (eio == NULL)
		return NULL;

	eio->run = 0;

	//just a hint for kernel
	eio->epfd = epoll_create(max);

	if (-1 == eio->epfd) 
		goto error;

	max = (max >= 64 ? max : 64);

	eio->max_events = max;
	eio->poll_evs = (struct epoll_event*) calloc(max, sizeof(struct epoll_event));
	eio->eio_evs = (struct eio_event*) calloc(max, sizeof(struct eio_event));

	eio->hz = DEFAULT_EIO_LOOP_HZ;

	return eio;
error:
	free(eio);
	return NULL;
}


/** 
 * eio_loop_file_event
 */
int eio_loop_file_event(eio_loop *eio, int fd, int mask, int op, ev_file_proc proc, void* context) {

	assert(eio != NULL);
	assert(fd > 0);
	assert(eio->max_events > fd);

//  assert(((op & EIO_EVENT_CLEAR) && mask == EIO_NONE) || (!(op & EIO_EVENT_CLEAR) && mask != EIO_NONE));

	struct epoll_event ev = {0};
	ev.data.fd = fd;        

	// get current event bitset
	ev.events = eio->eio_evs[fd].ev.file.ep_mask;

	// decide op on fd MOD or ADD
	int ep_ctl = 0;

//  ev.events |= EPOLLET;

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,17)
	ev.events |= EPOLLRDHUP;
#endif

	switch (op) {

		case EIO_EVENT_ADD:

			if (mask & EIO_WRITEABLE) {
				ev.events |= EPOLLOUT;
				eio->eio_evs[fd].ev.file.wproc = proc;
			}
			if (mask & EIO_READABLE) {
				ev.events |= EPOLLIN; 
				eio->eio_evs[fd].ev.file.rproc = proc;
			}
			if (mask & EIO_ERR)
				eio->eio_evs[fd].ev.file.errproc = proc;

			// add or update
			ep_ctl = eio->eio_evs[fd].mask ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;

			eio->eio_evs[fd].mask |= mask;
			eio->eio_evs[fd].ev.file.ep_mask = ev.events;

			break;

		case EIO_EVENT_DEL:
			assert(proc == NULL);

			if (eio->eio_evs[fd].mask == EIO_NONE)
				return eio_fail;

			// unregister
			if (mask & EIO_WRITEABLE) {
				ev.events &= ~EPOLLOUT;
				eio->eio_evs[fd].ev.file.wproc = NULL;
			}
			if (mask & EIO_READABLE) {
				ev.events &= ~EPOLLIN; 
				eio->eio_evs[fd].ev.file.rproc = NULL;
			}
			if (mask & EIO_ERR)
				eio->eio_evs[fd].ev.file.errproc = NULL;

			eio->eio_evs[fd].mask &= ~mask;
			eio->eio_evs[fd].ev.file.ep_mask = ev.events;

			// update or delete
			ep_ctl = (eio->eio_evs[fd].mask) ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;

			break;


		case EIO_EVENT_CLEAR:
			assert(proc == NULL);

			eio->eio_evs[fd].mask = EIO_NONE;
			eio->eio_evs[fd].ev.file.ep_mask = 0;

			// delete
			ep_ctl = EPOLL_CTL_DEL;

			break;

		case EIO_EVENT_GET:
			assert(proc == NULL);

			return eio->eio_evs[fd].mask;

	}

	eio->eio_evs[fd].user_data = context;

	/*
	 * we discard return -1 and error. because of duplicated register
	 * and epoll_ctl an existing file event. and for syscall performance 
	 * consideration. so we return -1 directly and throw it to user
	 *
	 * Note: Kernel < 2.6.9 requires a non null event pointer even for *EPOLL_CTL_DEL*
	 *
	 */
	return epoll_ctl(eio->epfd, ep_ctl, fd, &ev) != -1 ? eio->eio_evs[fd].mask : -1;
}



/**
 * eio_loop_destroy
 */
void eio_loop_destroy(eio_loop *eio) {
	assert(eio != NULL);
	assert(eio->run != 1);

	close(eio->epfd);
	free(eio);
}


/**
 * eio_loop_before_proc
 */
void eio_loop_before_proc(eio_loop *loop, eio_loop_before proc) {
	assert(loop != NULL);

	loop->before = proc;
}


inline void eio_loop_stop(eio_loop *eio) {
	eio->run = 0;
}

static int poll_time_event(eio_loop* eio) {
	return 0;
}


static int poll_file_event(eio_loop* eio) {

	int n = epoll_wait(eio->epfd, eio->poll_evs, eio->max_events, 1000 / eio->hz);

	// if we encounter the SIGTRAP , gdb/pstack 
	if (n <= 0 && errno == EINTR)
		return 0;

	if (0 == eio->poll_evs[0].data.fd && errno == EINTR) 
		return 0;

	for (int i=0; i!=n; i++) {

		struct eio_event *event = &eio->eio_evs[eio->poll_evs[i].data.fd];

		// events may not be cared in this time
		// discard in this time !!
		if (0 == event->mask)
			continue;

		uint32_t happen = eio->poll_evs[i].events;

		if ((happen & EPOLLERR) || (happen & EPOLLHUP)
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,17)
			|| (happen & EPOLLRDHUP)
#endif
		   ) {

			// for ERROR
			if (event->ev.file.errproc) {
				event->ev.file.errproc(eio, eio->poll_evs[i].data.fd, EIO_ERR, event->user_data);
				// don't trigger EPOLLIN
				continue;
			}
		} 

		if ((happen & EPOLLIN) && event->ev.file.rproc) {
			// for READ
//			event->mask &= ~EIO_READABLE;
//			event->ev.file.ep_mask &= ~EPOLLIN;
			event->ev.file.rproc(eio, eio->poll_evs[i].data.fd, EIO_READABLE, event->user_data);
		} 
		if ((eio->poll_evs[i].events & EPOLLOUT) && event->ev.file.wproc) {
			// for WRITE
//			event->mask &= ~EIO_WRITEABLE;
//			event->ev.file.ep_mask &= ~EPOLLOUT;
			event->ev.file.wproc(eio, eio->poll_evs[i].data.fd, EIO_WRITEABLE, event->user_data);
		}
	}

	return n;
}

/**
 * eio_loop_run
 */
eio_loop* eio_loop_run(eio_loop *eio) {

	assert(eio != NULL);

	eio->run = 1;

	// do an infinite loop for epoll_wait
	while(eio->run) {

		if (eio->before != NULL)
			eio->before(eio);

		/**
		 * check the timer event
		 */
		eio->stats.timer_events += poll_time_event(eio);

		/**
		 * check the timer event
		 */
		eio->stats.file_events += poll_file_event(eio);

	}

	return eio;
}

