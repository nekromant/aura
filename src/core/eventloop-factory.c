#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <aura/list.h>
#include <aura/eventloop.h>
#include <aura/aura.h>
#include <aura/slog.h>

static LIST_HEAD(loops);
static struct aura_eventloop_module *current_loop_module;

void aura_eventloop_module_register(struct aura_eventloop_module *m)
{
    list_add_tail(&m->linkage, &loops);
}

/**
 * Retrieve the currently selected eventloop module plugin
 *
 * @return Pointer to the current aura_eventloop_module structure
 */
const struct aura_eventloop_module *aura_eventloop_module_get()
{
	if (!current_loop_module)
		BUG(NULL, "Internal bug: No eventloop module set as current");
	current_loop_module->usage++;
	return current_loop_module;
}

/**
 * Select an underlying eventsystem plugin to use. Available: epoll, libevent
 * WARNING: It is highly recommended to use only ONE type of eventsystem in your
 * app. This function affects the whole library.
 *
 * @param  name [description]
 * @return      [description]
 */
int aura_eventloop_module_select(const char *name)
{
	struct aura_eventloop_module *pos;

	list_for_each_entry(pos, &loops, linkage)
	if (strcmp(pos->name, name) == 0) {
		if (current_loop_module && current_loop_module->usage)
			slog(0, SLOG_WARN, "Using multiple eventloop modules in the same application is a bad idea");
		current_loop_module = pos;
		return 0;
	}

	return -EIO;
}

static void __attribute__((constructor (102))) init_factory() {
		char *evtloop = getenv("AURA_USE_EVENTLOOP");
		if (evtloop)
			aura_eventloop_module_select(evtloop);
		else
			aura_eventloop_module_select("libevent");

		/* If for some reason we couldn't select libevent, pick the first one
		 * that can be used.
		 */
	    if (!current_loop_module && !list_empty(&loops))
			current_loop_module = list_entry(loops.next, struct aura_eventloop_module, linkage);

		/* If we still have no eventloop module, we're screwed */

		if (!current_loop_module)
			BUG(NULL, "Failed to select default eventloop module, check env variable AURA_USE_EVENTLOOP (%s)", evtloop);
}
