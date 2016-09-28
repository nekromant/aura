#ifndef AURA_EVENTLOOP_H
#define AURA_EVENTLOOP_H


struct aura_node;
struct aura_eventloop;
struct timeval;
/** \addtogroup loop
 *  @{
 */

/**
 * Create an eventloop from one or more nodes.
 * e.g. aura_eventloop_create(node1, node2, node3);
 * @param ... one or more struct aura_node*
 */
#define aura_eventloop_create(...) \
	aura_eventloop_create__(0, ## __VA_ARGS__, NULL)


/**
 *
 * @}
 */


void aura_eventloop_destroy(struct aura_eventloop *loop);
void *aura_eventloop_vcreate(va_list ap);
void *aura_eventloop_create__(int dummy, ...);
void *aura_eventloop_create_empty();
void aura_eventloop_add(struct aura_eventloop *loop, struct aura_node *node);
void aura_eventloop_del(struct aura_node *node);
void aura_eventloop_dispatch(struct aura_eventloop *loop, int flags);
void aura_eventloop_loopexit(struct aura_eventloop *loop, struct timeval *tv);


#endif /* end of include guard: AURA_EVENTLOOP_H */
