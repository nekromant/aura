#ifndef AURA_PRIVATE_H
#define AURA_PRIVATE_H

uint64_t aura_platform_timestamp();
void aura_process_node_event(struct aura_node *node, const struct aura_pollfds *fd);

/* Event-System Backend */
void *aura_eventsys_backend_create();
void aura_eventsys_backend_destroy(void *backend);
int aura_eventsys_backend_wait(void *backend, int timeout_ms);
void aura_eventsys_backend_fd_action(void *backend, const struct aura_pollfds *ap, int action);
void aura_process_node_event(struct aura_node *node, const struct aura_pollfds *fd);

/* Transport Plugins API */
void aura_transport_register(struct aura_transport *tr);
void aura_transport_dump_usage();
void aura_transport_release(const struct aura_transport *tr);

static inline void  aura_set_transportdata(struct aura_node *node, void *udata)
{
	node->transport_data = udata;
}

static inline void *aura_get_transportdata(struct aura_node *node)
{
	return node->transport_data;
}


#endif
