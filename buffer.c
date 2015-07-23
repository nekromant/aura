#include <aura/aura.h>
#include <aura/private.h>

struct aura_buffer *aura_buffer_internal_request(int size);
void aura_buffer_internal_free(struct aura_buffer *buf);

 /** \addtogroup bufapi
 * @{
 */


/**
 * Request an aura_buffer for this node big enough to contain at least size bytes of data.
 * If the node transport overrides buffer allocation - transport-specific allocation functions
 * will be called.
 *
 * @param nd
 * @param size
 * @return
 */
struct aura_buffer *aura_buffer_request(struct aura_node *nd, int size)
{
	struct aura_buffer *ret; 
	if (!nd->tr->buffer_request)
		ret = aura_buffer_internal_request(size);	
	else
		ret = nd->tr->buffer_request(nd, size);
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
		

