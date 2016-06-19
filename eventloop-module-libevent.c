#include <stdlib.h>
#include <aura/aura.h>
#include <aura/private.h>
#include <aura/eventloop.h>
#include <aura/list.h>
#include <aura/timer.h>
#include <event.h>

struct aura_libevent_timer {
	struct aura_timer	timer;
	struct event		evt;
};

static int libevent_create(struct aura_eventloop *loop)
{
	slog(3, SLOG_WARN, "evtsys-libevent: Using experimental libevent backend");
	struct event_base *ebase = event_base_new();
	if (!ebase)
		return -ENOMEM;

	aura_eventloop_moduledata_set(loop, ebase);
	return 0;
}

void libevent_destroy(struct aura_eventloop *loop)
{
	struct event_base *ebase = aura_eventloop_moduledata_get(loop);

	event_base_free(ebase);
}

static void dispatch_cb_fn(evutil_socket_t fd, short evt, void *arg)
{
	struct aura_pollfds *ap = arg;
	aura_node_dispatch_event(ap->node, NODE_EVENT_DESCRIPTOR, ap);
}

static void libevent_fd_action(
	struct aura_eventloop *		loop,
	const struct aura_pollfds *	app,
	int				action)
{
	struct aura_pollfds *ap = (struct aura_pollfds *)app;
	struct aura_node *node = ap->node;
	struct event_base *ebase = aura_eventloop_moduledata_get(loop);

	ap->magic = 0xdeadbeaf;
	if (ap->eventsysdata) {
		/* TODO: Error checking */
		event_del(ap->eventsysdata);
		event_free(ap->eventsysdata);
		ap->eventsysdata = NULL;
	}

	if (action == AURA_FD_ADDED) {
		int ret;
		ap->eventsysdata = event_new(ebase, ap->fd,
					     ap->events | EV_PERSIST,
					     dispatch_cb_fn, ap);
		if (!ap->eventsysdata)
			BUG(node, "evtsys-libevent: Failed to create event");
		ret = event_add(ap->eventsysdata, NULL);
		if (0 != ret)
			BUG(NULL, "evtsys-libevent: Failed to add event to base");
	}
}

static void libevent_dispatch(struct aura_eventloop *loop, int flags)
{
	struct event_base *ebase = aura_eventloop_moduledata_get(loop);
	int libevent_flags = 0;

	if (flags & AURA_EVTLOOP_NONBLOCK)
		libevent_flags |= EVLOOP_NONBLOCK;
	if (flags & AURA_EVTLOOP_ONCE)
		libevent_flags |= EVLOOP_ONCE;

	if (-1 == event_base_loop(ebase, libevent_flags))
		BUG(NULL, "event_base_loop() returned -1");
}

static void libevent_loopbreak(struct aura_eventloop *loop, struct timeval *tv)
{
	struct event_base *ebase = aura_eventloop_moduledata_get(loop);

	if (0 != event_base_loopexit(ebase, tv))
		BUG(NULL, "event_base_loopexit() failed!");
}

static void libevent_node_added(struct aura_eventloop *loop, struct aura_node *node)
{
	/* Nothing to do here */
}

static void libevent_node_removed(struct aura_eventloop *loop, struct aura_node *node)
{
	/* Nothing to do here */
}

static void libevent_timer_create(struct aura_eventloop *loop, struct aura_timer *tm)
{
	/* Nothing to do here */
}

static void timer_dispatch_fn(int fd, short events, void *arg)
{
	struct aura_timer *tm = arg;

	aura_timer_dispatch(tm);
	
}

static void libevent_timer_start(struct aura_eventloop *loop, struct aura_timer *tm)
{
	struct aura_libevent_timer *ltm;
	struct event_base *ebase = aura_eventloop_moduledata_get(loop);

	ltm = container_of(tm, struct aura_libevent_timer, timer);
	int flags = 0;
	if (tm->flags & AURA_TIMER_PERIODIC)
		flags |= EV_PERSIST;
	event_assign(&ltm->evt, ebase, -1, flags, timer_dispatch_fn, tm);
	event_add(&ltm->evt, &tm->tv);
}

static void libevent_timer_stop(struct aura_eventloop *loop, struct aura_timer *tm)
{
	struct aura_libevent_timer *ltm;

	ltm = container_of(tm, struct aura_libevent_timer, timer);
	event_del(&ltm->evt);
}

static void libevent_timer_destroy(struct aura_eventloop *loop, struct aura_timer *tm)
{
	/* Nothing to do */
}

static struct aura_eventloop_module levt =
{
	.name		= "libevent",
	.timer_size	= sizeof(struct aura_libevent_timer),
	.timer_create	= libevent_timer_create,
	.timer_start	= libevent_timer_start,
	.timer_stop	= libevent_timer_stop,
	.timer_destroy	= libevent_timer_destroy,
	.create		= libevent_create,
	.destroy	= libevent_destroy,
	.fd_action	= libevent_fd_action,
	.dispatch	= libevent_dispatch,
	.loopbreak	= libevent_loopbreak,
	.node_added	= libevent_node_added,
	.node_removed	= libevent_node_removed,
};

AURA_EVENTLOOP_MODULE(levt);
