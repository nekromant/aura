#include <aura/aura.h>


/* FixMe: The current timeout handling is pretty naive. But let it be so for now */

static void eventloop_recalculate_timeouts(struct aura_eventloop *loop)
{
	loop->poll_timeout = 5000;
	struct aura_node *pos; 
	list_for_each_entry(pos, &loop->nodelist, eventloop_node_list) {
		if (pos->poll_timeout < loop->poll_timeout)
			loop->poll_timeout = pos->poll_timeout;
	}
	slog(4, SLOG_DEBUG, "Adjusted event poll timeout to %d ms", pos->poll_timeout);
}

static void eventloop_fd_changed_cb(const struct aura_pollfds *fd, enum aura_fd_action act, void *arg)
{
	struct aura_eventloop *loop = arg;
	aura_eventsys_backend_fd_action(loop->eventsysdata, fd, act);
}

void aura_eventloop_add(struct aura_eventloop *loop, struct aura_node *node)
{
	const struct aura_pollfds *fds;
	int i, count; 

	/* Some sanity checking first */
	if (aura_eventsys_get_data(node) != NULL)
		BUG(node, "Specified node already has an event-system");

	/* Link our next node into our list and adjust timeouts */
	list_add_tail(&node->eventloop_node_list, &loop->nodelist);
	aura_eventsys_set_data(node, loop);

	if (loop->poll_timeout > node->poll_timeout)
		loop->poll_timeout = node->poll_timeout;
		
	/* Now we need to fetch all descriptrs and add them to the loop */
	count = aura_get_pollfds(node, &fds);
	for (i=0; i<count; i++) 
		aura_eventsys_backend_fd_action(loop->eventsysdata, &fds[i], AURA_FD_ADDED);

	/* Set up our fdaction callback to handle descriptor changes */ 	
	aura_fd_changed_cb(node, eventloop_fd_changed_cb, loop); 
}

void aura_eventloop_del(struct aura_eventloop *loop, struct aura_node *node)
{
	const struct aura_pollfds *fds;
	int i, count; 

	/* Some sanity checking first */
	if (aura_eventsys_get_data(node) == NULL)
		BUG(node, "Specified node is not bound to any eventsystem");	

	/* Remove our node from the list */
	list_del(&node->eventloop_node_list);
	aura_eventsys_set_data(node, NULL);

	/* Recalc all timeouts */ 
	eventloop_recalculate_timeouts(loop);

	/* Remove all descriptors */
	count = aura_get_pollfds(node, &fds);
	for (i=0; i<count; i++) 
		aura_eventsys_backend_fd_action(loop->eventsysdata, &fds[i], AURA_FD_REMOVED);

	/* Remove our fd_changed callback */ 
	aura_fd_changed_cb(node, NULL, NULL); 
}

void *aura_eventloop_vcreate(va_list ap)
{
	struct aura_node *node; 
	struct aura_eventloop *loop = calloc(1, sizeof(*loop));

	loop->poll_timeout = 5000; 
	if (!loop)
		return NULL;

	loop->eventsysdata = aura_eventsys_backend_create();
	if (!loop->eventsysdata)
		goto err_free_loop;

	INIT_LIST_HEAD(&loop->nodelist); 

	/* Add all our nodes to this loop */
	while ((node = va_arg(ap, struct aura_node *))) 
		aura_eventloop_add(loop, node);

	/* Just return the loop, we're good */
	return loop;

err_free_loop:
	free(loop);
	return NULL; 
}

void *aura_eventloop_create__(int dummy, ...)
{
	void *ret;
	va_list ap; 
	va_start(ap, dummy);
	ret = aura_eventloop_vcreate(ap); 
	va_end(ap);
	return ret;
}


void aura_handle_events(struct aura_eventloop *loop)
{
	aura_handle_events_timeout(loop, -1); 
}

void aura_handle_events_timeout(struct aura_eventloop *loop, int timeout_ms)
{
	struct aura_node *pos; 
	/* Handle any pending events from descriptors */
	aura_eventsys_backend_wait(loop->eventsysdata, timeout_ms); 
	/* Check if any nodes needs their periodic poll */ 

	list_for_each_entry(pos, &loop->nodelist, eventloop_node_list) {
		aura_process_node_event(pos, NULL);
	}
}
