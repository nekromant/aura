#include <aura/aura.h>
#include <aura/private.h>
#include <inttypes.h>


static void *aura_eventsys_get_autocreate(struct aura_node *node)
{
	struct aura_eventloop *loop = aura_eventloop_get_data(node);
	if (loop == NULL) {
		slog(3, SLOG_DEBUG, "aura: Auto-creating eventsystem for node");
		loop = aura_eventloop_create(node);
		if (!loop) {
			slog(0, SLOG_ERROR, "aura: eventloop auto-creation failed");
			aura_panic(node);
		}
		loop->autocreated = 1;
		aura_eventloop_set_data(node, loop);
	}
	return loop;
}

/**
 * \addtogroup node
 * @{
 */

/**
 * Open an aura node and return the instance.
 *
 * @param name the name of the transport (e.g. usb)
 * @param ap va_list of transport arguments. See transport docs.
 * @return pointer to the newly created node
 */
struct aura_node *aura_vopen(const char* name, va_list ap)
{
	struct aura_node *node = calloc(1, sizeof(*node));
	int ret = 0; 
	if (!node)
		return NULL;
	node->poll_timeout = 250; /* 250 ms default */
	node->tr = aura_transport_lookup(name); 
	if (!node->tr) { 
		slog(0, SLOG_FATAL, "Invalid transport name: %s", name);
		goto err_free_node;
	}

	INIT_LIST_HEAD(&node->outbound_buffers);
	INIT_LIST_HEAD(&node->inbound_buffers);
	INIT_LIST_HEAD(&node->event_buffers);

	node->status = AURA_STATUS_OFFLINE;

	/*  Eventsystem will be either lazy-initialized or created via 
	 *  aura_eventloop_* functions 
	 */

	if (node->tr->open)
		ret = node->tr->open(node, ap);

	if (ret != 0) 
		goto err_free_node;
	
	slog(6, SLOG_LIVE, "Created a node using transport: %s", name); 
	return node;

err_free_node:
	slog(0, SLOG_FATAL, "Error opening transport: %s", name);
	free(node);
	return NULL;
}

/**
 * Open a remote node. Transport arguments are passed in a variadic fasion.
 * The number and format is transport-dependent.
 * @param name transport name
 * @return node instance or NULL
 */
struct aura_node *aura_open(const char *name, ...)
{
	struct aura_node *ret; 
	va_list ap;
	va_start(ap, name);	
	ret = aura_vopen(name, ap);
	va_end(ap);
	return ret; 
}


static void cleanup_buffer_queue(struct list_head *q)
{
	int i = 0;

	struct list_head *pos, *tmp;
	list_for_each_safe(pos, tmp, q) {
		struct aura_buffer *b; 
		b = list_entry(pos, struct aura_buffer, qentry); 
		list_del(pos);
		aura_buffer_release(NULL, b);
		i++;
	}
	slog(6, SLOG_LIVE, "Cleaned up %d buffers", i);
}


/**
 * Close the node and free memory.
 *
 * @param node
 */
void aura_close(struct aura_node *node)
{
	struct aura_eventloop *loop = aura_eventloop_get_data(node); 
	
	if (node->tr->close)
		node->tr->close(node);

	if (loop) { 
		if (loop->autocreated)
			aura_eventloop_destroy(loop);
		else
			aura_eventloop_del(node);
	}

	/* After transport shutdown we need to clean up 
	   remaining buffers */
	cleanup_buffer_queue(&node->inbound_buffers);
	cleanup_buffer_queue(&node->outbound_buffers);
	aura_transport_release(node->tr);
	/* Check if we have an export table registered and nuke it */
	if (node->tbl)
		aura_etable_destroy(node->tbl);

	/* Free file descriptors */
	if (node->fds)
		free(node->fds);
	
	free(node);
	slog(6, SLOG_LIVE, "Transport closed");
}

/**
 * @}
 */


static void aura_handle_inbound(struct aura_node *node)
{
	while(1) {
		struct aura_buffer *buf;
		struct aura_object *o;	

		buf = aura_dequeue_buffer(&node->inbound_buffers); 
		if (!buf)
			break;
		o = buf->userdata;
		/* 
		 * userdata will now contain pointer to node, to make
		 * aura_buffer_get calls simpler 
		 */
		buf->userdata = node; 
		slog(4, SLOG_DEBUG, "Handling %s id %d (%s)", 
		     object_is_method(o) ? "response" : "event", 
		     o->id, o->name);

		if (object_is_method(o) && !o->pending) { 
			slog(0, SLOG_WARN, "Dropping orphan call result %d (%s)", 
			     o->id, o->name);
			aura_buffer_release(node, buf);
		} else if (o->calldonecb) { 
			slog(4, SLOG_DEBUG, "Callback for method/event %d (%s)",
			     o->id, o->name);
			o->calldonecb(node, AURA_CALL_COMPLETED, buf, o->arg);
			aura_buffer_release(node, buf);
		} else if (object_is_method(o) && (node->sync_call_running)) { 
			slog(4, SLOG_DEBUG, "Completing call for method %d (%s)",
			     o->id, o->name);
			node->sync_call_result = AURA_CALL_COMPLETED;
			node->sync_ret_buf = buf; 
			o->pending--;
			if (o->pending < 0)
				BUG(node, "Internal BUG: pending evt count lesser than zero");
		} else {
			/* This one is tricky. We have an event with no callback */
			if (node->sync_event_max > 0) { /* Queue it up into event_queue if it's enabled */
				/* If we have an overrun - drop the oldest event */
				if (node->sync_event_max <= node->sync_event_count) {
					struct aura_buffer *todrop;
					const struct aura_object *dummy;
					int ret = aura_get_next_event(node, &dummy, &todrop);
					if (ret != 0)
						BUG(node, "Internal bug");
					aura_buffer_release(node, todrop);
				}

				/* Now just queue the next one */
				aura_queue_buffer(&node->event_buffers, buf);
				node->sync_event_count++;
				slog(4, SLOG_DEBUG, "Queued event %d (%s) for sync readout", 
				     o->id, o->name);
			} else {
				slog(0, SLOG_WARN, "Dropping event %d (%s)",
				     o->id, o->name);
				aura_buffer_release(node, buf);
			}
		}
	}
}

/**
 * \addtogroup async
 * @{
 */

/**
 * Setup the status change callback. This callback will be called when
 * the node goes online and offline
 *
 * @param node
 * @param cb
 * @param arg
 */
void aura_status_changed_cb(struct aura_node *node, 
			    void (*cb)(struct aura_node *node, int newstatus, void *arg),
			    void *arg)
{
	node->status_changed_arg = arg;
	node->status_changed_cb = cb;
}

/**
 * Set the fd changed callback. You don't need this call unless you're using your
 * own eventsystem.
 *
 * @param node
 * @param cb
 * @param arg
 */
void aura_fd_changed_cb(struct aura_node *node, 
			void (*cb)(const struct aura_pollfds *fd, enum aura_fd_action act, void *arg),
			void *arg)
{
	node->fd_changed_arg = arg;
	node->fd_changed_cb = cb;
}

/**
 * Set up etable changed callback.
 * @param node
 * @param cb
 * @param arg
 */
void aura_etable_changed_cb(struct aura_node *node, 
			    void (*cb)(struct aura_node *node, void *arg),
			    void *arg)
{
	node->etable_changed_arg = arg;
	node->etable_changed_cb = cb;
}


/**
 * Queue a call for object with id id to the node.
 * Normally you do not need this function - use aura_call() and aura_call_raw()
 * synchronous calls and aura_start_call() and aura_start_call_raw() for async.
 *
 * @param node
 * @param id
 * @param calldonecb
 * @param arg
 * @param buf
 * @return
 */
int aura_queue_call(struct aura_node *node, 
		    int id,
		    void (*calldonecb)(struct aura_node *dev, int status, struct aura_buffer *ret, void *arg),
		    void *arg,
		    struct aura_buffer *buf)
{
	struct aura_object *o;
	struct aura_eventloop *loop = aura_eventsys_get_autocreate(node);
	int isfirst;

	if(node->status != AURA_STATUS_ONLINE) 
		return -ENOEXEC;

	o = aura_etable_find_id(node->tbl, id);
	if (!o)
		return -EBADSLT;
		
	if (o->pending) 
		return -EIO; 

	isfirst = list_empty(&node->outbound_buffers);

	o->calldonecb = calldonecb; 
	o->arg = arg; 
	buf->userdata = o;
	o->pending++;
	aura_queue_buffer(&node->outbound_buffers, buf);
	slog(4, SLOG_DEBUG, "Queued call for id %d (%s), notifying node", id, o->name);

	if (isfirst) {
		slog(4, SLOG_DEBUG, "Notifying transport of queue status change");
		node->last_checked = 0;
		aura_eventloop_interrupt(loop);
	};

	return 0;
}

/**
 * Start a call for the object identified by its id the export table.
 * Upon completion the specified callback will be fired with call results.
 *
 * @param node
 * @param id
 * @param calldonecb
 * @param arg
 * @return -EBADSLT if the requested id is not in etable
 * 		   -EIO if serialization failed or another synchronous call for this id is pending
 * 		   -ENOEXEC if the node is currently offline
 */
int aura_start_call_raw(
	struct aura_node *node, 
	int id,
	void (*calldonecb)(struct aura_node *dev, int status, struct aura_buffer *ret, void *arg),
	void *arg,
	...)
{
	va_list ap;
	struct aura_buffer *buf; 
	struct aura_object *o = aura_etable_find_id(node->tbl, id);
	if (!o)
		return -EBADSLT;

	va_start(ap, arg);
	buf = aura_serialize(node, o->arg_fmt, ap);
	va_end(ap);
	if (!buf) 
		return -EIO;
	
	return aura_queue_call(node, id, calldonecb, arg, buf);
}

/**
 * Set the callback that will be called when event with supplied id arrives.
 * NULL calldonecb disables this event callback.
 * aura_buffer supplied to called in 'ret' contains any data assiciated with this event.
 * You should not free the data buffer in the callback or access the buffer from anywhere
 * but this callback. Make a copy if you need it.
 *
 * @param node
 * @param id
 * @param calldonecb
 * @param arg
 * @return
 */
int aura_set_event_callback_raw(
	struct aura_node *node,
	int id,
	void (*calldonecb)(struct aura_node *dev, int status, struct aura_buffer *ret, void *arg),
	void *arg)
{
	struct aura_object *o = aura_etable_find_id(node->tbl, id);
	if (!o)
		return -EBADSLT;

	if (!object_is_event(o))
		return -EBADF;

	o->calldonecb = calldonecb;
	o->arg = arg;
	return 0;
}

/**
 * Set the callback that will be called when event with supplied name arrives.
 * NULL calldonecb disables this event callback.
 * aura_buffer supplied to called in 'ret' contains any data assiciated with this event.
 * You should not free the data buffer in the callback or access the buffer from anywhere
 * but this callback. Make a copy if you need it.
 *
 * @param node
 * @param event
 * @param calldonecb
 * @param arg
 * @return
 */
int aura_set_event_callback(
	struct aura_node *node,
	const char *event,
	void (*calldonecb)(struct aura_node *dev, int status, struct aura_buffer *ret, void *arg),
	void *arg)
{
	struct aura_object *o = aura_etable_find(node->tbl, event);
	if (!o)
		return -EBADSLT;

	if (!object_is_event(o))
		return -EBADF;

	o->calldonecb = calldonecb;
	o->arg = arg;
	return 0;
}

/**
 * Start a call to an object identified by name.
 * @param node
 * @param name
 * @param calldonecb
 * @param arg
 * @return -EBADSLT if the requested id is not in etable
 * 		   -EIO if serialization failed or another synchronous call for this id is pending
 * 		   -ENOEXEC if the node is currently offline
 */
int aura_start_call(
	struct aura_node *node, 
	const char *name,
	void (*calldonecb)(struct aura_node *dev, int status, struct aura_buffer *ret, void *arg),
	void *arg,
	...)
{
	struct aura_object *o; 
	va_list ap;
	struct aura_buffer *buf; 
	o = aura_etable_find(node->tbl, name);
	if (!o) 
		return -ENOENT; 

	va_start(ap, arg);
	buf = aura_serialize(node, o->arg_fmt, ap);
	va_end(ap);
	if (!buf) 
		return -EIO;
	
	return aura_queue_call(node, o->id, calldonecb, arg, buf);
}

/**
 * @}
 * \addtogroup sync
 * @{
 */


/**
 * Block until node's status becomes one of the requested
 *
 * @param node
 * @param status
 */
void aura_wait_status(struct aura_node *node, int status)
{
	struct aura_eventloop *loop = aura_eventsys_get_autocreate(node);
	while (node->status != status)
		aura_handle_events(loop);
}


/**
 * Synchronously call an object identified by id.
 * If the call succeeds, retbuf will be the pointer to aura_buffer containing
 * the values. It's your responsibility to call aura_buffer_release() on the retbuf
 * after you are done working with resulting values
 *
 * @param node
 * @param id
 * @param retbuf
 * @return
 */
int aura_call_raw(
	struct aura_node *node, 
	int id,
	struct aura_buffer **retbuf,
	...)
{
	va_list ap;
	struct aura_buffer *buf; 
	int ret; 
	struct aura_object *o = aura_etable_find_id(node->tbl, id);
	struct aura_eventloop *loop = aura_eventsys_get_autocreate(node);

	if (node->sync_call_running) 
		BUG(node, "Internal bug: Synchronos call within a synchronos call");

	if (!o)
		return -EBADSLT;

	if (!loop)
		BUG(node, "Node has no assosiated event system. Fix your code!");
	
	va_start(ap, retbuf);
	buf = aura_serialize(node, o->arg_fmt, ap);
	va_end(ap);

	if (!buf) {
		slog(2, SLOG_WARN, "Serialization failed");
		return -EIO;
	}

	node->sync_call_running = true; 
	
	ret = aura_queue_call(node, id, NULL, NULL, buf);
	if (ret) 
		return ret;
	
	while (node->tbl->objects[id].pending)
		aura_handle_events(loop);
	
	*retbuf =  node->sync_ret_buf;
	node->sync_call_running = false; 
	return node->sync_call_result;
}

/**
 * Synchronously call a remote method of node identified by name. 
 * If the call succeeds, retbuf will be the pointer to aura_buffer containing
 * the values. It's your responsibility to call aura_buffer_release() on the retbuf
 * after you are done working with resulting values
 *
 * @param node
 * @param name
 * @param retbuf
 * @return
 */
int aura_call(
	struct aura_node *node, 
	const char *name,
	struct aura_buffer **retbuf,
	...)
{
	va_list ap;
	struct aura_buffer *buf; 
	int ret; 
	struct aura_object *o = aura_etable_find(node->tbl, name);
	struct aura_eventloop *loop = aura_eventsys_get_autocreate(node);
	
	if (node->sync_call_running) 
		BUG(node, "BUG: Synchronous call within a synchronous call - fix your code!");

	if (!o)
		return -EBADSLT;

	if (!loop)
		BUG(node, "Node has no associated event system - fix your code!");
	
	va_start(ap, retbuf);
	buf = aura_serialize(node, o->arg_fmt, ap);
	va_end(ap);

	if (!buf) {
		slog(2, SLOG_WARN, "Serialization failed");
		return -EIO;
	}

	node->sync_call_running = true; 
	
	ret = aura_queue_call(node, o->id, NULL, NULL, buf);
	if (ret) 
		return ret;
	
	while (o->pending)
		aura_handle_events(loop);
	
	*retbuf =  node->sync_ret_buf;
	node->sync_call_running = false; 
	return node->sync_call_result;
}


/**
 * Enable synchronous event processing.
 *
 * Call this function to make the node queue up to count events in an internal buffer
 * to be read out. By default the node does not queue any events for synchronous readout and
 * drops them immediately if no callbacks are installed to catch this event.
 * If the number of events in this queue reaches count - events will be dropped (oldest first)
 *
 * To disable synchronous event processing completely - call this function with count=0
 *
 * Adding a callback for an event is not recommended if you use this API, although possible.
 * It will just prevent events from being queued here - instead your callback will be fired.
 *
 * If there are more than count events already queued - all extra events will be immediately
 * discarded.
 *
 * @param node
 * @param count Maximum number of events to store for synchronous readout
 */
void aura_enable_sync_events(struct aura_node *node, int count)
{
	while(node->sync_event_max >= count) {
		const struct aura_object *o;
		struct aura_buffer *buf;
		int ret = aura_get_next_event(node, &o, &buf);
		if (ret!=0)
			BUG(node, "Internal bug while resizing event queue (failed to drop some events)");
		aura_buffer_release(node, buf);
	}
	node->sync_event_max = count;
}

/**
 * Get the number of events currently in internal event queue.
 *
 * @param node
 * @return
 */
int aura_get_pending_events(struct aura_node *node)
{
	return node->sync_event_count;
}

/**
 * Retrieve the next event from the synchronous event queue. If there is no events in queue -
 * this function may block until the next event arrives.
 *
 * If the node goes offline during waiting for event this function will return an error
 *
 * The caller should not in any way modify or free the obj pointer. The obj pointer returned will
 * may not be valid after the next synchronous call (e.g. if the node went offline and back online)
 * so do not rely on that in your application.
 *
 * The caller should free the retbuf pointer with aura_buffer_release when it is no longer
 * needed
 *
 * @param node
 * @param obj
 * @param retbuf
 * @return 0 if the event has been read out.
 */
int aura_get_next_event(struct aura_node *node, const struct aura_object ** obj, struct aura_buffer **retbuf)
{
	struct aura_eventloop *loop = aura_eventsys_get_autocreate(node);

	while (!node->sync_event_count) {
		aura_handle_events(loop);
	}

	*retbuf = aura_dequeue_buffer(&node->event_buffers);
	if (!(*retbuf))
		aura_panic(node);

	*obj = (const struct aura_object *)(*retbuf)->userdata;
	node->sync_event_count--;
	return 0;
}

/**
 * @}
 */

void *aura_eventloop_get_data(struct aura_node *node)
{
	return node->eventsys_data;
}

void aura_eventloop_set_data(struct aura_node *node, void *data)
{
	node->eventsys_data = data;
}


/**
 * \addtogroup trapi
 * @{
 */


/**
 * Change node status.
 * This function should be called by transport plugins whenever the node
 * changes the status
 *
 * @param node
 * @param status
 */
void aura_set_status(struct aura_node *node, int status)
{
	int oldstatus = node->status;
	node->status = status;

	if (oldstatus == status)
		return;

	if ((oldstatus == AURA_STATUS_OFFLINE) && (status == AURA_STATUS_ONLINE)) {
		/* Dump etable */
		int i;
		slog(2, SLOG_INFO, "Node %s is now going online", node->tr->name);
		slog(2, SLOG_INFO, "--- Dumping export table ---");
		for (i=0; i< node->tbl->next; i++) {
			slog(2, SLOG_INFO, "%d. %s %s %s(%s )  [out %d bytes] | [in %d bytes] ",
			     node->tbl->objects[i].id,
			     object_is_method((&node->tbl->objects[i])) ? "METHOD" : "EVENT ",
			     node->tbl->objects[i].ret_pprinted,
			     node->tbl->objects[i].name,
			     node->tbl->objects[i].arg_pprinted,
			     node->tbl->objects[i].arglen,
			     node->tbl->objects[i].retlen);
		}
		slog(1, SLOG_INFO, "-------------8<-------------");
	}
	if ((oldstatus == AURA_STATUS_ONLINE) && (status == AURA_STATUS_OFFLINE)) {
		int i;

		slog(2, SLOG_INFO, "Node %s going offline, clearing outbound queue",
		     node->tr->name);
		cleanup_buffer_queue(&node->outbound_buffers);
		/* Handle any remaining inbound messages */
		aura_handle_inbound(node);
		/* Cancel any pending calls */
		for (i=0; i < node->tbl->next; i++) {
			struct aura_object *o;
			o=&node->tbl->objects[i];
			if (o->pending && o->calldonecb) {
				o->calldonecb(node, AURA_CALL_TRANSPORT_FAIL, NULL, o->arg);
				o->pending--;
			}
		}
		/* If any of the synchronos calls are running - inform them */
		node->sync_call_result = AURA_CALL_TRANSPORT_FAIL;
		node->sync_ret_buf = NULL;
	}

	if (node->status_changed_cb)
		node->status_changed_cb(node, status, node->status_changed_arg);
}

/**
 * Set node endianness.
 * This function should be called by transport when it knows the remote
 * endianness before any of the actual calls are made.
 *
 * @param node
 * @param en
 */
void aura_set_node_endian(struct aura_node *node, enum aura_endianness en)
{
	if (aura_get_host_endianness() != en)
		node->need_endian_swap = true;
}
/**
 * @}
 */

/**
 * Process an event for the node.
 * If it is just periodic polling fd can be NULL. This
 *
 * @param node
 * @param fd
 */
void aura_process_node_event(struct aura_node *node, const struct aura_pollfds *fd)
{
	if (fd && node->tr->loop)
		node->tr->loop(node, fd);

	uint64_t curtime = aura_platform_timestamp();

	if ((curtime - node->last_checked > node->poll_timeout) && node->tr->loop) {
		node->tr->loop(node, NULL);
		node->last_checked = curtime;
	}

	/* Now grab all we got from the inbound queue and fire the callbacks */
	aura_handle_inbound(node);
}


