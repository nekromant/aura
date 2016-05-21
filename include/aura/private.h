#ifndef AURA_PRIVATE_H
#define AURA_PRIVATE_H

enum node_event {
		NODE_EVENT_HAVE_OUTBOUND,
		NODE_EVENT_HAVE_INBOUND,
		NODE_EVENT_PERIODIC,
		NODE_EVENT_DESCRIPTOR
};

enum node_buffer_queue
{
	NODE_QUEUE_OUTBOUND,
	NODE_QUEUE_INBOUND
};

#define AURA_BUFFER_MAGIC_ID 0xdeadc0de
/* raw calls */
int aura_core_start_call (struct aura_node *node,
			  struct aura_object *o,
			  void(*calldonecb)(struct aura_node *dev, int status,
					    struct aura_buffer *ret, void *arg),
			  void *arg,
			  struct aura_buffer *buf);

int aura_core_call (struct aura_node *node,
		    struct aura_object *o,
		    struct aura_buffer **retbuf,
		    struct aura_buffer *argbuf);


uint64_t aura_platform_timestamp();
void aura_process_node_event(struct aura_node *node, const struct aura_pollfds *fd);

int aura_node_buffer_pool_gc_once(struct aura_node *pos);
int aura_node_buffer_pool_gc_full(struct aura_node *pos);

/* Transport Plugins API */
void aura_transport_register(struct aura_transport *tr);
void aura_transport_dump_usage();
void aura_transport_release(const struct aura_transport *tr);
void aura_call_fail(struct aura_node *node, struct aura_object *o);
void aura_eventloop_report_event(struct aura_eventloop *loop, enum node_event evt, struct aura_pollfds *ap);


void aura_queue_buffer(struct list_head *list, struct aura_buffer *buf);
struct aura_buffer *aura_dequeue_buffer(struct list_head *head);
void aura_requeue_buffer(struct list_head *head, struct aura_buffer *buf);
struct aura_buffer *aura_peek_buffer(struct list_head *head);
void aura_node_queue_write(struct aura_node *node,
		     enum node_buffer_queue queue_type,
			 struct aura_buffer *buf);
struct aura_buffer *aura_node_queue_read(struct aura_node *node,
			 	enum node_buffer_queue queue_type);
#endif
