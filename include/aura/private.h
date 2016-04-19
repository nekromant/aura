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

/* Event-System Backend */
void *aura_eventsys_backend_create(void *loopdata);
void aura_eventsys_backend_destroy(void *backend);
int aura_eventsys_backend_wait(void *backend, int timeout_ms);
void aura_eventsys_backend_interrupt(void *backend);
struct event_base *aura_eventsys_backend_get_ebase(void *backend);
void aura_eventsys_backend_fd_action(void *backend, const struct aura_pollfds *ap, int action);
void aura_process_node_event(struct aura_node *node, const struct aura_pollfds *fd);
void aura_eventloop_interrupt(struct aura_eventloop *loop);

void aura_eventloop_report_event(struct aura_eventloop *loop, struct aura_pollfds *ap);

/* Transport Plugins API */
void aura_transport_register(struct aura_transport *tr);
void aura_transport_dump_usage();
void aura_transport_release(const struct aura_transport *tr);
void aura_call_fail(struct aura_node *node, struct aura_object *o);
void aura_eventloop_set_data(struct aura_node *node, struct aura_eventloop *);


#endif
