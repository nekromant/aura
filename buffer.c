#include <aura/aura.h>
#include <aura/private.h>

#define AURA_USE_BUFFER_POOL

struct aura_buffer *aura_buffer_internal_request(int size);
void aura_buffer_internal_free(struct aura_buffer *buf);

 /** \addtogroup bufapi
 * @{
 */


/* Let's see if we have a buffer in our pool here */
static struct aura_buffer *fetch_buffer_from_pool(struct aura_node *nd, int size)
{
	struct aura_buffer *buf = NULL;
	struct list_head *pos, *tmp;
	list_for_each_safe(pos, tmp, &nd->buffer_pool) {
		buf = list_entry(pos, struct aura_buffer, qentry);
		if (buf->size >= size) {
			list_del(pos);
			nd->num_buffers_in_pool--;
			return buf;
		}
	}
	return NULL;
}

/**
 * Request an aura_buffer for this node big enough to contain at least size bytes of data.
 *
 * The buffer's userdata will be set to point to the  
 *
 * 
 * If the node transport overrides buffer allocation - transport-specific allocation function
 * will be called.
 *
 * @param nd
 * @param size
 * @return
 */
struct aura_buffer *aura_buffer_request(struct aura_node *nd, int size)
{
	struct aura_buffer *ret = NULL;
	int act_size = sizeof(struct aura_buffer) + size;

	act_size += nd->tr->buffer_overhead;
#ifdef AURA_USE_BUFFER_POOL
	/* Try buffer pool first */
	ret = fetch_buffer_from_pool(nd, size);
#endif
	/* Fallback to alloc() */
	if (!ret) {
		if (!nd->tr->buffer_request)
			ret = aura_buffer_internal_request(act_size);
		else
			ret = nd->tr->buffer_request(nd, act_size);
	}
	ret->magic = AURA_BUFFER_MAGIC_ID;
	ret->size = act_size - sizeof(struct aura_buffer);
	ret->owner = nd; 
	aura_buffer_rewind(ret);
	return ret;
}

/**
 * Release an aura_buffer, returning it back to the node's buffer pool.
 * Aura call with garbage-collect the buffer pool later
 *
 * @param nd
 * @param buf
 */
void aura_buffer_release(struct aura_buffer *buf)
{
	struct aura_node *nd = buf->owner;
	if (buf->magic != AURA_BUFFER_MAGIC_ID)
		BUG(nd, "FATAL: Attempting to release a buffer with invalid magic OR double free an aura_buffer");

	/* Just put the buffer back into the pool */
#ifdef AURA_USE_BUFFER_POOL
	list_add_tail(&buf->qentry, &nd->buffer_pool);
	nd->num_buffers_in_pool++;
#else
	aura_buffer_destroy(buf);
#endif
}


/**
 * Force aura to immediately free the buffer, bypassing the node's buffer pool.
 * Do not call this function directly, unless you know what you are doing - use
 * aura_buffer_release() instead.
 */
void aura_buffer_destroy(struct aura_buffer *buf)
{
	struct aura_node *nd = buf->owner;
	if (buf->magic != AURA_BUFFER_MAGIC_ID)
		BUG(nd, "FATAL: Attempting to destroy a buffer with invalid magic OR double free an aura_buffer");
	buf->magic = 0;

	if (nd && nd->tr->buffer_release)
		nd->tr->buffer_release(buf);
	else
		aura_buffer_internal_free(buf);
}

/**
 * @}
 */
