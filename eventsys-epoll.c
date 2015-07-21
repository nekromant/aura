#include <aura/aura.h>
#include <aura/private.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <unistd.h>

struct aura_epoll_data { 
	int epollfd; 
};

void *aura_eventsys_backend_create()
{
	struct aura_epoll_data *epd = calloc(1, sizeof(*epd));
	if (!epd)
		return NULL; 

	epd->epollfd = epoll_create(7);
	if (!epd->epollfd)
		goto errfreeepd;

	return epd;
errfreeepd:
	free(epd);
	return NULL;
}


void aura_eventsys_backend_destroy(void *backend)
{
	struct aura_epoll_data *epd = backend;
	close(epd->epollfd); 
	free(epd);
}

void aura_eventsys_backend_fd_action(void *backend, const struct aura_pollfds *ap, int action)
{
	struct aura_node *node = ap->node; 
	struct epoll_event ev;
	struct aura_epoll_data *epd = backend;
	int ret; 
	int op = (action == AURA_FD_ADDED) ? EPOLL_CTL_ADD : EPOLL_CTL_DEL;

	ev.events = 0; /* FixMe: ... */
	ev.data.ptr = (void *) ap;
	ret = epoll_ctl(epd->epollfd, op, ap->fd, &ev);
	if (ret != 0) 
		BUG(node, "Event System failed to add/remove a descriptor");
}

#define NUM_EVTS 8
int aura_eventsys_backend_wait(void *backend, int timeout_ms) 
{
	struct aura_epoll_data *epd = backend;
	struct aura_node *node; 
	struct aura_pollfds *ap;
	struct epoll_event ev[NUM_EVTS];
	int i; 

	int ret = epoll_wait(epd->epollfd, ev, NUM_EVTS, timeout_ms);

	slog(4, SLOG_LIVE, "Epoll reported %d events", ret);
	if (ret < 0) { 
		slog(0, SLOG_ERROR, "Epoll returned -1: %s", strerror(errno));
		aura_panic(NULL);
		return -1; /* Never reached */
	}

	for (i = 0; i< ret; i++) {
		ap = ev[i].data.ptr;
		node = ap->node;
		aura_process_node_event(node, ap);
	}

	return ret;
}
