#include <aura/aura.h>
#include <event.h>
#include <aura/private.h>

struct aura_libevent_data {
	struct aura_pollfds	evtfd;
	void *			loopdata;
	struct event_base *	ebase;
	struct event *		ievt;
};

static void interrupt_cb_fn(evutil_socket_t fd, short evt, void *arg)
{
	struct aura_libevent_data *epd = arg;
	aura_eventloop_report_event(epd->loopdata, NULL);
	event_base_loopbreak(epd->ebase);
}

void *aura_eventsys_backend_create(void *loopdata)
{
	slog(3, SLOG_WARN, "evtsys-libevent: Using experimental libevent backend");
	struct aura_libevent_data *epd = calloc(1, sizeof(*epd));

	if (!epd)
		return NULL;
	epd->loopdata = loopdata;
	epd->ebase = event_base_new();

	if (!epd->ebase)
		goto errfreeepd;

	epd->ievt = event_new(epd->ebase, -1, EV_READ,
		interrupt_cb_fn, epd);
	if (!epd->ievt)
		goto errepolldestroy;

	return epd;

errepolldestroy:
	event_base_free(epd->ebase);
errfreeepd:
	free(epd);
	return NULL;
}

void aura_eventsys_backend_destroy(void *backend)
{
	struct aura_libevent_data *epd = backend;
	event_del(epd->ievt);
	event_free(epd->ievt);
	event_base_free(epd->ebase);
	free(epd);
}

static void dispatch_cb_fn(evutil_socket_t fd, short evt, void *arg)
{
	struct aura_pollfds *ap = arg;
	struct aura_node *node = ap->node;
	struct aura_eventloop *l = aura_eventloop_get_data(node);
	struct aura_libevent_data *epd = l->eventsysdata;

	aura_eventloop_report_event(epd->loopdata, ap);

}
void aura_eventsys_backend_fd_action(
	void *backend,
	const struct aura_pollfds *app,
	int action)
{
	struct aura_pollfds *ap = (struct aura_pollfds *) app;
	struct aura_node *node = ap->node;
	struct aura_libevent_data *epd = backend;
	ap->magic = 0xdeadbeaf;

	if (ap->eventsysdata) {
		/* TODO: Error checking */
		event_del(ap->eventsysdata);
		event_free(ap->eventsysdata);
		ap->eventsysdata = NULL;
	}

	if (action == AURA_FD_ADDED) {
		ap->eventsysdata = event_new(epd->ebase, ap->fd, ap->events | EV_PERSIST,
									dispatch_cb_fn, ap);
		if (!ap->eventsysdata)
			BUG(node, "evtsys-libevent: Failed to create event");
		event_add(ap->eventsysdata, NULL);
	}
}

#define NUM_EVTS 1
int aura_eventsys_backend_wait(void *backend, int timeout_ms)
{
	struct aura_libevent_data *epd = backend;

	struct timeval delay;
	delay.tv_usec = timeout_ms*1000;
	delay.tv_sec = 0;

	if (0 != event_add(epd->ievt, &delay))
		return BUG(NULL, "evtsys-libevent: Failed to add event");


	int ret = event_base_dispatch(epd->ebase);
	if (-1 == ret)
		return BUG(NULL, "event_base_dispatch returned -1");

	return ret;
}

void aura_eventsys_backend_interrupt(void *backend)
{
	struct aura_libevent_data *epd = backend;
	event_active(epd->ievt, EV_READ, 0);
}

struct event_base *aura_eventsys_backend_get_ebase(void *backend)
{
	struct aura_libevent_data *epd = backend;
	return epd->ebase;
}
