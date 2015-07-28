#ifndef AURA_PRIVATE_H
#define AURA_PRIVATE_H

uint64_t aura_platform_timestamp();
void aura_process_node_event(struct aura_node *node, const struct aura_pollfds *fd);

/* Event-System Backend */
void *aura_eventsys_backend_create(void *loopdata);
void aura_eventsys_backend_destroy(void *backend);
int aura_eventsys_backend_wait(void *backend, int timeout_ms);
void aura_eventsys_backend_interrupt(void *backend);
void aura_eventsys_backend_fd_action(void *backend, const struct aura_pollfds *ap, int action);
void aura_process_node_event(struct aura_node *node, const struct aura_pollfds *fd);


void aura_eventloop_report_event(struct aura_eventloop *loop, struct aura_pollfds *ap);

/* Transport Plugins API */
void aura_transport_register(struct aura_transport *tr);
void aura_transport_dump_usage();
void aura_transport_release(const struct aura_transport *tr);

/**
 * \addtogroup trapi
 * @{
 */


/** Set transport-specific data for this node
 * @param node
 * @param udata
 */
static inline void  aura_set_transportdata(struct aura_node *node, void *udata)
{
	node->transport_data = udata;
}

/** Get transport-specific data for this node
 * @param node
 * @param udata
 */
static inline void *aura_get_transportdata(struct aura_node *node)
{
	return node->transport_data;
}

/**
 * @}
 */

/* event system data access functions */
void *aura_eventloop_get_data(struct aura_node *node);
void aura_eventloop_set_data(struct aura_node *node, void *data);

/* Buffer stuff */

/**
 * \addtogroup bufapi
 * @{
 */

/**
 * Reposition the internal pointer of the buffer buf to the start of serialized data.
 * This function takes buffer_offset of the node's transport into account
 *
 * @param node
 * @param buf
 */
static inline void aura_buffer_rewind(struct aura_node *node, struct aura_buffer *buf) {
	buf->pos = node->tr->buffer_offset;
}

/**
 * @}
 */


#endif
