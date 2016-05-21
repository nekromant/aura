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
	#ifdef AURA_DEFAULT_EVENTLOOP
		aura_eventloop_module_select(AURA_DEFAULT_EVENTLOOP);
	#else
		/* Assume epoll as default for now */
		aura_eventloop_module_select("libevent");
	#endif
}
