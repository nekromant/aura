#include <aura/aura.h>
#include <aura/timer.h>

static struct aura_timer *tm;
static int exitcode = -1;

static void timer_cb_fn(struct aura_node *node, struct aura_timer *timer, void *arg)
{
	if (timer != tm)
		BUG(NULL, "Internal timer fuckup detected");
	if ((int) arg != 567)
		BUG(node, "Unexpected arg received: %x/%x", arg, 567);
	aura_eventloop_loopexit(aura_node_eventloop_get(node), NULL);
	exitcode = 0;
}

int main() {
	slog_init(NULL, 18);
	struct timeval tv;

	struct aura_node *n = aura_open("dummy", NULL);
	if (!n)
		return -1;

	slog(0, SLOG_DEBUG, "node ptr %x", n);
	/* Make sure we have an auto-created eventsystem assigned */
	aura_wait_status(n, AURA_STATUS_ONLINE);

	tv.tv_sec = 1;
	tv.tv_usec = 0;
	/* These timers will be bound to an auto-created eventsystem */
	tm = aura_timer_create(n, timer_cb_fn, (void *) 567);
	slog(0, SLOG_DEBUG, "%x/%x", tm->callback, tm->callback_arg);
	aura_timer_start(tm, 0, &tv);

	/* And should be moved to the new eventsystem here */
	struct aura_eventloop *loop = aura_eventloop_create(n);

	tv.tv_sec = 5;
	tv.tv_usec = 0;
	//Double-calling loopexit() causes memory leak in libevent
	//aura_eventloop_loopexit(loop, &tv);
	aura_eventloop_dispatch(loop, 0);
	aura_close(n);
	aura_eventloop_destroy(loop);
	return exitcode;
}
