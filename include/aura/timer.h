#ifndef AURA_TIMER_H
#define AURA_TIMER_H

#define AURA_TIMER_PERIODIC (1<<0)
#define AURA_TIMER_FREE  (1<<1)

struct aura_eventloop;
struct aura_node;

typedef void (*aura_timer_cb_fn)(struct aura_node *node, void *arg);

struct aura_timer {
	struct timeval tv;
	struct list_head entry; /* For node linked list */
	struct aura_eventloop *loop;
	const struct aura_eventloop_module *module;
	struct aura_node *node;
	int flags;
	bool is_active;
	aura_timer_cb_fn callback;
	void *callback_arg;
};

void aura_timer_update(struct aura_timer *tm, aura_timer_cb_fn timer_cb_fn, void *arg);
struct aura_timer *aura_timer_create(struct aura_node *node, aura_timer_cb_fn timer_cb_fn, void *arg);
void aura_timer_start(struct aura_timer *tm, int flags, struct timeval *tv);
void aura_timer_stop(struct aura_timer *timer);
void aura_timer_destroy(struct aura_timer *timer);

#endif
