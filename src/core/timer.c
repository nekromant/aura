#include <stdbool.h>
#include <time.h>
#include <aura/aura.h>
#include <aura/timer.h>
#include <aura/eventloop.h>

void aura_timer_update(struct aura_timer *tm, aura_timer_cb_fn timer_cb_fn, void *arg)
{
	struct aura_eventloop *loop = aura_node_eventloop_get(tm->node);
	if (!loop)
		BUG(tm->node, "Internal bug: Node has no associated eventsystem");
	if (loop->module != tm->module)
		BUG(tm->node, "Please, call aura_eventloop_module_select() BEFORE anything else.");

	tm->callback = timer_cb_fn;
	tm->callback_arg = arg;
}

struct aura_timer *aura_timer_create(struct aura_node *node, aura_timer_cb_fn timer_cb_fn, void *arg)
{
	struct aura_eventloop *loop = aura_node_eventloop_get_autocreate(node);
	struct aura_timer *tm = calloc(1, loop->module->timer_size);
	if (!tm)
		BUG(node, "FATAL: Memory allocation failure");
	tm->module = loop->module; /* Store current eventloop module in the name of sanity */
	tm->node = node;
	loop->module->timer_create(loop, tm);
	aura_timer_update(tm, timer_cb_fn, arg);
	list_add_tail(&tm->entry, &node->timer_list);
	slog(0, SLOG_DEBUG, "Create %x", tm);
	return tm;
}

void aura_timer_start(struct aura_timer *tm, int flags, struct timeval *tv)
{
	struct aura_eventloop *loop = aura_node_eventloop_get(tm->node);
	if (tm->is_active) {
		slog(0, SLOG_WARN, "Tried to activate a timer that's already active. Doing nothing");
		return;
	}

	if (!loop)
		BUG(tm->node, "Internal bug: Node has no associated eventsystem");

	tm->flags = flags;
	if (tv)
		tm->tv = *tv;
	tm->module->timer_start(loop, tm);
	tm->is_active = true;
}

void aura_timer_stop(struct aura_timer *timer)
{
	if (!timer->is_active)
		return;

	struct aura_eventloop *loop = aura_node_eventloop_get(timer->node);
	if (!loop)
		BUG(timer->node, "Internal bug: Node has no associated eventsystem");
	timer->module->timer_stop(loop, timer);
	timer->is_active = false;
}

void aura_timer_destroy(struct aura_timer *timer)
{
	struct aura_eventloop *loop = aura_node_eventloop_get(timer->node);
	if (!loop)
		BUG(timer->node, "Internal bug: Node has no associated eventsystem");
	if (timer->is_active)
		aura_timer_stop(timer);
	timer->module->timer_destroy(loop, timer);
	list_del(&timer->entry);
	free(timer);
}

bool aura_timer_is_active(struct aura_timer *timer)
{
	return timer->is_active;
}

void aura_timer_dispatch(struct aura_timer *tm)
{
	if (!(tm->flags & AURA_TIMER_PERIODIC))
		tm->is_active = false;

	if (tm->callback)
		tm->callback(tm->node, tm, tm->callback_arg);

	if (tm->flags & AURA_TIMER_FREE)
		aura_timer_destroy(tm);
}
