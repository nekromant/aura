#include <aura/aura.h>
#include <aura/private.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/eventfd.h>

struct aura_epoll_data { 
	int epollfd; 
	struct aura_pollfds evtfd;
	void *loopdata; 
};

void *aura_eventsys_backend_create(void *loopdata)
{
	struct aura_epoll_data *epd = calloc(1, sizeof(*epd));
	if (!epd)
		return NULL; 
	epd->loopdata = loopdata;
	epd->epollfd = epoll_create(7);
	if (!epd->epollfd)
		goto errfreeepd;

	/* We use eventfd here to interrupt things */
 
	epd->evtfd.fd = eventfd(0, EFD_NONBLOCK);
	epd->evtfd.events = EPOLLIN;
	if (-1 == epd->evtfd.fd) 
		goto errepolldestroy;

	aura_eventsys_backend_fd_action(epd, &epd->evtfd, AURA_FD_ADDED);	
	
	return epd;
errepolldestroy:
	close(epd->epollfd); 	
errfreeepd:
	free(epd);
	return NULL;
}

void aura_eventsys_backend_destroy(void *backend)
{
	struct aura_epoll_data *epd = backend;
	close(epd->epollfd); 
	close(epd->evtfd.fd);
	free(epd);
}

void aura_eventsys_backend_fd_action(void *backend, const struct aura_pollfds *ap, int action)
{
	struct aura_node *node = ap->node; 
	struct epoll_event ev;
	struct aura_epoll_data *epd = backend;
	int ret; 
	int op = (action == AURA_FD_ADDED) ? EPOLL_CTL_ADD : EPOLL_CTL_DEL;
	slog(4, SLOG_DEBUG, "epoll: Descriptor %d added to epoll", ap->fd);
	ev.events = ap->events; 
	ev.data.ptr = (void *) ap;
	ret = epoll_ctl(epd->epollfd, op, ap->fd, &ev);
	if (ret != 0) 
		BUG(node, "Event System failed to add/remove a descriptor");
}

#define NUM_EVTS 8
int aura_eventsys_backend_wait(void *backend, int timeout_ms) 
{
	struct aura_epoll_data *epd = backend;
	struct aura_pollfds *ap;
	struct epoll_event ev[NUM_EVTS];
	int i; 

	int ret = epoll_wait(epd->epollfd, ev, NUM_EVTS, timeout_ms);

	slog(4, SLOG_LIVE, "epoll: reported %d events", ret);
	if (ret < 0) { 
		slog(0, SLOG_ERROR, "epoll: returned -1: %s", strerror(errno));
		aura_panic(NULL);
		return -1; /* Never reached */
	}

	for (i = 0; i< ret; i++) {
		ap = ev[i].data.ptr;
		if (ap == &epd->evtfd) { 
			/* Read out our event */
			uint64_t tmp; 
			ap = NULL;
			read(epd->evtfd.fd, &tmp, sizeof(uint64_t));
		}
		aura_eventloop_report_event(epd->loopdata, ap); 	
	}
	return ret;
}

void aura_eventsys_backend_interrupt(void *backend)
{
	struct aura_epoll_data *epd = backend;
	uint64_t tmp = 1;
	write(epd->evtfd.fd, &tmp, sizeof(uint64_t));	
}
