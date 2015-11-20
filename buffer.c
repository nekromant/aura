#include <aura/aura.h>
#include <aura/private.h>

struct aura_buffer *aura_buffer_internal_request(int size);
void aura_buffer_internal_free(struct aura_buffer *buf);

 /** \addtogroup bufapi
 * @{
 */


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
	struct aura_buffer *ret; 
	int act_size = sizeof(struct aura_buffer) + size;

	act_size += nd->tr->buffer_overhead;

	if (!nd->tr->buffer_request)
		ret = aura_buffer_internal_request(act_size);	
	else
		ret = nd->tr->buffer_request(nd, act_size);
	ret->magic = AURA_BUFFER_MAGIC_ID;
	ret->size = act_size - sizeof(struct aura_buffer);
	ret->owner = nd; 
	aura_buffer_rewind(ret);
	return ret;
}

/**
 * Release an aura_buffer.
 * If the node transport overrides memory allocation - transport-specific buffer_relase method
 * will be used.
 *
 * @param nd
 * @param buf
 */
void aura_buffer_release(struct aura_node *nd, struct aura_buffer *buf)
{
	if (buf->magic != AURA_BUFFER_MAGIC_ID)
		BUG(nd, "FATAL: Attempting to free a buffer with invalid magic OR double free an aura_buffer");
	buf->magic = 0;

	if (nd && nd->tr->buffer_release)
		nd->tr->buffer_release(nd, buf);
	else
		aura_buffer_internal_free(buf);	
}

/**
 * @}
 */

#define aura_buffer_put(buf, data) {					\
		memcpy(&buf->data[buf->pos], data, sizeof(data));	\
		buf->pos += sizeof(data)				\
			}						\
		

