#ifndef AURA_PRIVATE_H
#define AURA_PRIVATE_H


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

void aura_process_node_event(struct aura_node *node, const struct aura_pollfds *fd);
void aura_eventloop_interrupt(struct aura_eventloop *loop);
int aura_node_buffer_pool_gc_once(struct aura_node *pos);
int aura_node_buffer_pool_gc_full(struct aura_node *pos);

/* Transport Plugins API */
void aura_transport_register(struct aura_transport *tr);
void aura_transport_dump_usage();
void aura_transport_release(const struct aura_transport *tr);
void aura_call_fail(struct aura_node *node, struct aura_object *o);
void aura_eventloop_set_data(struct aura_node *node, struct aura_eventloop *);


#endif
