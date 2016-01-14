#include <aura/aura.h>
#include <aura/private.h>
#include <sys/eventfd.h>

/* FixMe: The current timeout handling is pretty naive. But let it be so for now */

 /** \addtogroup loop
 *  @{
 */


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

/**
 * Add a node to existing event loop.
 *
 * WARNING: This node should not be registered in any other manually created event loops or a panic 
 * will occur. 
 *
 * @param loop
 * @param node
 */
void aura_eventloop_add(struct aura_eventloop *loop, struct aura_node *node)
{
	struct aura_eventloop *curloop = aura_eventloop_get_data(node);

	/* Some sanity checking first */
	if ((curloop != NULL) && (!curloop->autocreated))
		BUG(node, "Specified node is already bound to an event-system");

	if (curloop != NULL) { 
		slog(4, SLOG_DEBUG, "eventloop: Node has an associated auto-created eventsystem, destroying...");
		aura_eventloop_destroy(curloop);
	}

	/* Link our next node into our list and adjust timeouts */
	list_add_tail(&node->eventloop_node_list, &loop->nodelist);
	aura_eventloop_set_data(node, loop);

	if (loop->poll_timeout > node->poll_timeout)
		loop->poll_timeout = node->poll_timeout;

	/* Set up our fdaction callback to handle descriptor changes */ 	
	aura_fd_changed_cb(node, eventloop_fd_changed_cb, loop); 
}

/**
 * Remove a node from it's associated event loop.
 *
 * WARNING: If the node is not bound to any event loop a panic will occur
 * @param loop
 * @param node
 */
void aura_eventloop_del(struct aura_node *node)
{
	const struct aura_pollfds *fds;
	int i, count; 
	struct aura_eventloop *loop = aura_eventloop_get_data(node);

	/* Some sanity checking first */
	if (loop == NULL)
		BUG(node, "Specified node is not bound to any eventloop");	

	/* Remove our node from the list */
	list_del(&node->eventloop_node_list);
	aura_eventloop_set_data(node, NULL);

	/* Recalc all timeouts */ 
	eventloop_recalculate_timeouts(loop);

	/* Remove all descriptors from epoll, but keep 'em in the node */
	count = aura_get_pollfds(node, &fds);
	for (i=0; i<count; i++) 
		aura_eventsys_backend_fd_action(loop->eventsysdata, &fds[i], AURA_FD_REMOVED);

	/* Remove our fd_changed callback */ 
	aura_fd_changed_cb(node, NULL, NULL); 
}


/** 
 * Create an empty eventloop with no nodes
 * 
 * @return Pointer to eventloop object or NULL
 */
void *aura_eventloop_create_empty()
{
	struct aura_eventloop *loop = calloc(1, sizeof(*loop));

	if (!loop)
		return NULL;
	
	loop->poll_timeout = 5000; 
	loop->eventsysdata = aura_eventsys_backend_create(loop);
	if (!loop->eventsysdata)
		goto err_free_loop;

	INIT_LIST_HEAD(&loop->nodelist); 

	/* Just return the loop, we're good */
	return loop;

err_free_loop:
	free(loop);
	return NULL; 	
}

/**
 * Create an event loop from a NULL-terminated list of nodes passed in va_list
 *
 * @param ap
 */
void *aura_eventloop_vcreate(va_list ap)
{
	struct aura_node *node; 

	struct aura_eventloop *loop = aura_eventloop_create_empty();

	if (!loop)
		return NULL;

	/* Add all our nodes to this loop */
	while ((node = va_arg(ap, struct aura_node *))) 
		aura_eventloop_add(loop, node);

	/* Return the loop, we're good */
	return loop;
}

/**
 * Create an event loop from a list of null-terminated nodes.
 * Do not use this function. See aura_eventloop_create() macro
 * @param dummy a dummy parameter required by va_start
 * @param ... A NULL-terminated list of nodes
 */
void *aura_eventloop_create__(int dummy, ...)
{
	void *ret;
	va_list ap; 
	va_start(ap, dummy);
	ret = aura_eventloop_vcreate(ap); 
	va_end(ap);
	return ret;
}

/** 
 * Destroy and eventloop and deassociate any nodes from it. 
 * 
 * @param loop 
 */
void aura_eventloop_destroy(struct aura_eventloop *loop) 
{
	struct list_head *pos; 
	struct list_head *tmp; 

	list_for_each_safe(pos, tmp, &loop->nodelist) {
		struct aura_node *node = list_entry(pos, struct aura_node, 
						    eventloop_node_list);
		aura_eventloop_del(node);
	}

	aura_eventsys_backend_destroy(loop->eventsysdata);
	free(loop);
}

/**
 * Handle events in the specified loop forever or 
 * until someone calls aura_eventloop_break()
 *
 * @param loop
 */
void aura_handle_events_forever(struct aura_eventloop *loop)
{
	loop->keep_running = 1;
	while (loop->keep_running)
		aura_handle_events_timeout(loop, loop->poll_timeout); 
}

/**
 * Handle events in the specified loop for a default timeout
 * or until aura_eventloop_break()
 *
 * @param loop
 */

void aura_handle_events(struct aura_eventloop *loop)
{
	aura_handle_events_timeout(loop, loop->poll_timeout); 
}

/**
 * Wait and handle an event in the specified loop and return or 
 * just wait until a timeout occurs.
 *
 * timeout_ms equal to 0 makes this function non-blocking
 *
 * timeout_ms equal to -1 makes thsi function idential to aura_handle_events()
 *
 * @param loop
 * @param timeout_ms
 */
void aura_handle_events_timeout(struct aura_eventloop *loop, int timeout_ms)
{
	struct aura_node *pos; 
	/* Handle any pending events from descriptors */
	aura_eventsys_backend_wait(loop->eventsysdata, timeout_ms); 
	/* Check if any nodes needs their periodic medicine */ 
	list_for_each_entry(pos, &loop->nodelist, eventloop_node_list) {
		aura_process_node_event(pos, NULL);
#ifdef AURA_USE_BUFFER_POOL
		/* GC: Ditch one last buffer from pool if we have too many */
		if (pos->num_buffers_in_pool >= pos->gc_threshold) {
			struct aura_buffer *buf;
			pos->num_buffers_in_pool--;
			buf = list_entry(pos->buffer_pool.prev, struct aura_buffer, qentry);
			list_del(pos->buffer_pool.prev);
			aura_buffer_destroy(buf);
		}
#endif
	}
}

/** 
 * Interrupt a running event loop. 
 * This function can be called from the callbacks.  
 * 
 * @param loop 
 */
void aura_eventloop_break(struct aura_eventloop *loop)
{
	loop->keep_running = 0; 
	aura_eventloop_interrupt(loop);
}


/**
 * @}
 */


/** 
 * Internal function that causes eventsystem to stop waiting and reiterate 
 * all the nodes. Called by the core to notify transport that a new message
 * has been posted.
 *
 * Do not use it in your applications! Use aura_eventloop_break()
 * instead. 
 * 
 * @param loop
*/
void aura_eventloop_interrupt(struct aura_eventloop *loop)
{
	aura_eventsys_backend_interrupt(loop->eventsysdata);
}

/** 
 * Internal function called by the eventsystem to report an event on a descriptor ap. 
 * Reporting ap as NULL means that the event processing has been interrupted via 
 * aura_eventloop_interrupt()
 * 
 * @param loop 
 * @param ap 
 */
void aura_eventloop_report_event(struct aura_eventloop *loop, struct aura_pollfds *ap)
{
	struct aura_node *node; 
	if (ap) { 
		if (ap->magic != 0xdeadbeaf)
			BUG(NULL, "bad APFD: %x", ap);
		node = ap->node;
		aura_process_node_event(node, ap);
	} else {
		list_for_each_entry(node, &loop->nodelist, eventloop_node_list) {
			aura_process_node_event(node, NULL);
		}
	}
}
