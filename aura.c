#include <aura/aura.h>
#include <aura/private.h>

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
	node->poll_timeout = 250; /* 250 ms default */
	if (!node)
		return NULL;
	node->tr = aura_transport_lookup(name); 
	if (!node->tr) { 
		slog(0, SLOG_FATAL, "Invalid transport name: %s", name);
		goto err_free_node;
	}

	INIT_LIST_HEAD(&node->outbound_buffers);
	INIT_LIST_HEAD(&node->inbound_buffers);

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
	if (node->tr->close)
		node->tr->close(node);

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
	/* FixMe: Deregister from event loop */

	free(node);
	slog(6, SLOG_LIVE, "Transport closed");
}

/**
 * @}
 */

/**
 * \addtogroup workflow
 * @{
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
		} else if (o->calldonecb) { 
			o->calldonecb(node, AURA_CALL_COMPLETED, buf, o->arg);
		} else if (node->sync_call_running) { 
			node->sync_call_result = AURA_CALL_COMPLETED;
			node->sync_ret_buf = buf; 
		} else {
			slog(0, SLOG_WARN, "Dropping unhandled event %d (%s)", 
			     o->id, o->name);
		}
		o->pending--;
		/* Don't free buffer for synchronos calls */
		if (!node->sync_call_running)
			aura_buffer_release(node, buf);
	}	
}

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

	if(node->status != AURA_STATUS_ONLINE) 
		return -ENOEXEC;

	o = aura_etable_find_id(node->tbl, id);
	if (!o)
		return -EBADSLT;
		
	if (o->pending) 
		return -EIO; 

	o->calldonecb = calldonecb; 
	o->arg = arg; 
	buf->userdata = o;
	o->pending++;
	aura_queue_buffer(&node->outbound_buffers, buf);
	slog(4, SLOG_DEBUG, "Queued call for id %d (%s)", id, o->name); 
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
 * Synchronously call an object identified by id.
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
	struct aura_eventloop *loop = aura_eventsys_get_data(node);

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
 * Synchronously call a remote method identified by name
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
	struct aura_eventloop *loop = aura_eventsys_get_data(node);
	
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
		slog(2, SLOG_WARN, "Serialisation failed");
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
 * @}
 */

void *aura_eventsys_get_data(struct aura_node *node)
{
	return node->eventsys_data;
}

void aura_eventsys_set_data(struct aura_node *node, void *data)
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
 * Process an event for the node.
 * If it is just periodic polling fd can be NULL. This
 *
 * @param node
 * @param fd
 */

/**
 * @}
 */


void aura_process_node_event(struct aura_node *node, const struct aura_pollfds *fd)
{
	if (fd && node->tr->loop)
		node->tr->loop(node, fd);

	uint64_t curtime = aura_platform_timestamp();
	if ((curtime - node->last_checked < node->poll_timeout) && node->tr->loop) {
		node->tr->loop(node, NULL);
		node->last_checked = curtime;
	}

	/* Now grab all we got from the inbound queue and fire the callbacks */
	aura_handle_inbound(node);
}

void aura_wait_status(struct aura_node *node, int status)
{
	struct aura_eventloop *loop = aura_eventsys_get_data(node);
	while (node->status != status)
		aura_handle_events(loop);
}

