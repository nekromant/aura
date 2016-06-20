#include <stdlib.h>
#include <aura/aura.h>
#include <aura/private.h>
#include <aura/eventloop.h>
#include <aura/list.h>
#include <aura/timer.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <unistd.h>


struct aura_timerfd_timer {
	struct aura_timer	timer;
	int			fd;
};

struct aura_epoll_loop {
	int			epollfd;
	struct aura_pollfds	evtfd;
	int			exit_after_ms;
	struct timespec		ts_deadline;
};

static int lepoll_create(struct aura_eventloop *loop)
{
	int ret = 0;
	struct aura_epoll_loop *lp = calloc(1, sizeof(*lp));

	if (!lp)
		return -ENOMEM;

	lp->epollfd = epoll_create(1);
	if (lp->epollfd == -1) {
		ret = -errno;
		goto err_free_lp;
	}

	lp->evtfd.fd = eventfd(0, EFD_NONBLOCK);
	lp->evtfd.events = EPOLLIN;

	if (-1 == lp->evtfd.fd) {
		ret = -EFAULT;
		goto err_epoll_destroy;
	}

	aura_eventloop_moduledata_set(loop, lp);
	loop->module->fd_action(loop, &lp->evtfd, AURA_FD_ADDED);
	return 0;

err_epoll_destroy:
	close(lp->epollfd);
err_free_lp:
	free(lp);
	return ret;
}

void lepoll_destroy(struct aura_eventloop *loop)
{
	struct aura_epoll_loop *lp = aura_eventloop_moduledata_get(loop);

	close(lp->epollfd);
	close(lp->evtfd.fd);
	free(lp);
	aura_eventloop_moduledata_set(loop, NULL);
}

static void lepoll_fd_action(
	struct aura_eventloop *		loop,
	const struct aura_pollfds *	app,
	int				action)
{
	struct aura_epoll_loop *lp = aura_eventloop_moduledata_get(loop);
	struct aura_pollfds *ap = (struct aura_pollfds *)app;
	struct aura_node *node = ap->node;
	struct epoll_event ev;


	((struct aura_pollfds *)ap)->magic = 0xdeadbeaf;
	int ret;
	int op = (action == AURA_FD_ADDED) ? EPOLL_CTL_ADD : EPOLL_CTL_DEL;

	slog(4, SLOG_DEBUG, "epoll: Descriptor %d %s epoll",
	     ap->fd, (action == AURA_FD_ADDED) ? "added to" : "removed from");

	ev.events = ap->events;
	ev.data.ptr = (void *)ap;
	ret = epoll_ctl(lp->epollfd, op, ap->fd, &ev);
	if (ret != 0)
		BUG(node, "Event System failed to add/remove a descriptor");
}

static struct timespec clk_get()
{
	struct timespec ts;

	if (0 != clock_gettime(CLOCK_MONOTONIC, &ts))
		BUG(NULL, "clock_gettime() failed: %s", strerror(errno));
	return ts;
}

static struct timespec clk_add_ms(struct timespec src, int ms)
{
	struct timespec bc = src;

	src.tv_nsec += ms * 1000000;
	if (src.tv_nsec < bc.tv_nsec) {
		/* Handle overflows */
		src.tv_sec += 1;
		src.tv_nsec -= 1000000000UL;
	}
	return src;
}

#define timespecsub(a, b, result)                                                     \
	do {                                                                        \
		(result)->tv_sec = (a)->tv_sec - (b)->tv_sec;                             \
		(result)->tv_nsec = (a)->tv_nsec - (b)->tv_nsec;                          \
		if ((result)->tv_nsec < 0) {                                              \
			--(result)->tv_sec;                                                     \
			(result)->tv_nsec += 1000000000;                                        \
		}                                                                         \
	} while (0)

#define timespeccmp(a, b, CMP)                                                  \
	(((a)->tv_sec == (b)->tv_sec) ?                                             \
	 ((a)->tv_nsec CMP(b)->tv_nsec) :                                          \
	 ((a)->tv_sec CMP(b)->tv_sec))


static void lepoll_dispatch(struct aura_eventloop *loop, int flags)
{
	struct aura_epoll_loop *lp = aura_eventloop_moduledata_get(loop);
	int timeout_ms = 5000;          /* Assume 5 sec iterations for a start */
	bool should_loop = true;        /* And loop forever by default */

	if (flags & AURA_EVTLOOP_ONCE)
		should_loop = false;

	if (flags & AURA_EVTLOOP_NONBLOCK) {
		should_loop = false;
		timeout_ms = 0;
	}

	do {
		struct epoll_event ev;
		struct aura_pollfds *ap;
		int ret = epoll_wait(lp->epollfd, &ev, 1, timeout_ms);
		if ((ret == 0) && (lp->exit_after_ms)) {
			/* If we have no event, just adjust the timeout in a simple way */
			lp->exit_after_ms -= timeout_ms;
			if (!lp->exit_after_ms)
				break;
			timeout_ms = lp->exit_after_ms;
		} else if (ret == 1) {
			ap = ev.data.ptr;
			if (ap == &lp->evtfd) {
				/* Reset eventfd machinery */
				uint64_t tmp;
				int ret = read(lp->evtfd.fd, &tmp, sizeof(uint64_t));
				if (ret != sizeof(uint64_t))
					BUG(NULL, "Error reading from eventfd descriptor ");

				/* We've been interrupted via loopbreak. Should we break? */
				if (!lp->exit_after_ms)
					break;
				/* Or just adjust our timeout ? */
				timeout_ms = lp->exit_after_ms;
			} else if (ap->eventsysdata != NULL) {
				/* This must be a timer! Only timers have eventsysdata set here */
				struct aura_timerfd_timer *ftm = ap->eventsysdata;
				uint64_t expiry_count;
				int ret;
				ret = read(ftm->fd, &expiry_count, sizeof(expiry_count));

				if (ret != sizeof(expiry_count))
					BUG(NULL, "timerfd read failed(): %s\n", strerror(errno));
				if (expiry_count > 1)
					slog(0, SLOG_WARN, "timerfd expired more than once. Your system may be too slow");

				aura_timer_dispatch(ap->eventsysdata);
			} else {
				/* This is an actual descriptor from node */
				aura_node_dispatch_event(ap->node, NODE_EVENT_DESCRIPTOR, ap);
			}

			if (lp->exit_after_ms) {
				struct timespec ts = clk_get();
				if (timespeccmp(&ts, &lp->ts_deadline, >=))
					break;

				struct timespec towait;
				timespecsub(&lp->ts_deadline, &ts, &towait);
				lp->exit_after_ms = towait.tv_sec * 1000 + (towait.tv_nsec / 1000000) + 1;
				timeout_ms = lp->exit_after_ms;
			}
		} else if (ret == -1) {
			BUG(NULL, "epoll_wait() returned -1: %s", strerror(errno));
		}
	} while (should_loop);
}

static void lepoll_loopbreak(struct aura_eventloop *loop, struct timeval *tv)
{
	struct aura_epoll_loop *lp = aura_eventloop_moduledata_get(loop);
	int timeout_ms = 0;

	if (tv)
		timeout_ms = (tv->tv_sec * 1000) + (tv->tv_usec / 1000);
	lp->exit_after_ms = timeout_ms;
	lp->ts_deadline = clk_get();
	lp->ts_deadline = clk_add_ms(lp->ts_deadline, timeout_ms);

	uint64_t tmp = 1;
	write(lp->evtfd.fd, &tmp, sizeof(uint64_t));
}

static void lepoll_node_added(struct aura_eventloop *loop, struct aura_node *node)
{
	/* Nothing to do here */
}

static void lepoll_node_removed(struct aura_eventloop *loop, struct aura_node *node)
{
	/* Nothing to do here */
}

static void tfd_timer_create(struct aura_eventloop *loop, struct aura_timer *tm)
{
	struct aura_timerfd_timer *etm = container_of(tm, struct aura_timerfd_timer, timer);
	int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);

	if (fd == -1)
		BUG(NULL, "timerfd_create() failed: %s", strerror(errno));

	etm->fd = fd;
	struct aura_pollfds *fds = aura_add_pollfds(tm->node, fd, EPOLLIN);
	fds->eventsysdata = tm;
}

static void tfd_timer_start(struct aura_eventloop *loop, struct aura_timer *tm)
{
	struct aura_timerfd_timer *etm = container_of(tm, struct aura_timerfd_timer, timer);
	struct itimerspec its;
	int ret;

	bzero(&its, sizeof(its));

	its.it_value.tv_sec = tm->tv.tv_sec;
	its.it_value.tv_nsec = tm->tv.tv_usec * 1000;

	if (tm->flags & AURA_TIMER_PERIODIC) {
		its.it_interval.tv_sec = tm->tv.tv_sec;
		its.it_interval.tv_nsec = tm->tv.tv_usec * 1000;
	}

	ret = timerfd_settime(etm->fd, 0, &its, NULL);
	if (ret != 0)
		BUG(NULL, "timerfd_settime() faliled: %s", strerror(errno));
}

static void tfd_timer_stop(struct aura_eventloop *loop, struct aura_timer *tm)
{
	struct aura_timerfd_timer *etm = container_of(tm, struct aura_timerfd_timer, timer);
	struct itimerspec its;

	bzero(&its, sizeof(its));
	timerfd_settime(etm->fd, 0, &its, NULL);
}

static void tfd_timer_destroy(struct aura_eventloop *loop, struct aura_timer *tm)
{
	struct aura_timerfd_timer *etm = container_of(tm, struct aura_timerfd_timer, timer);

	aura_del_pollfds(tm->node, etm->fd);
	close(etm->fd);
}

static struct aura_eventloop_module lepoll =
{
	.name		= "epoll",
	.timer_size	= sizeof(struct aura_timerfd_timer),
	.timer_create	= tfd_timer_create,
	.timer_start	= tfd_timer_start,
	.timer_stop	= tfd_timer_stop,
	.timer_destroy	= tfd_timer_destroy,
	.create		= lepoll_create,
	.destroy	= lepoll_destroy,
	.fd_action	= lepoll_fd_action,
	.dispatch	= lepoll_dispatch,
	.loopbreak	= lepoll_loopbreak,
	.node_added	= lepoll_node_added,
	.node_removed	= lepoll_node_removed,
};

AURA_EVENTLOOP_MODULE(lepoll);
