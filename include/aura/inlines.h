#ifndef AURA_INLINES_H
#define AURA_INLINES_H


/**
 * \addtogroup node
 * @{
 */

 /** Set user data associated with this node.
  *  Just a convenient way to attach an arbitary pointer to this node.
  *  See aura_get_userdata()
  *
  * @param node
  * @param udata
  */
static inline void  aura_set_userdata(struct aura_node *node, void *udata)
{
	node->user_data = udata;
}

/**
 * Get user data associated with this node.
 * See aura_set_userdata()
 *
 * @param node
 */
static inline void *aura_get_userdata(struct aura_node *node)
{
	return node->user_data;
}

/** 
 *  Retrieve current node status. 
 * 
 * @param node 
 * 
 * @return 
 */

static inline int aura_get_status(struct aura_node *node) 
{
	return node->status;
}

/**
 * @}
 */


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
