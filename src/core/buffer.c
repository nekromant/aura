#include <aura/aura.h>
#include <aura/private.h>
#include <aura/buffer_allocator.h>

struct aura_buffer *aura_buffer_internal_request(int size);
void aura_buffer_internal_free(struct aura_buffer *buf);

/** \addtogroup bufapi
 * @{
 */

/* Let's see if we have a buffer in our pool here */
static struct aura_buffer *fetch_buffer_from_pool(struct aura_node *	nd,
						  int			size)
{
	struct list_head *pos, *tmp;

	list_for_each_safe(pos, tmp, &nd->buffer_pool){
		struct aura_buffer *buf = list_entry(pos, struct aura_buffer, qentry);
		if (buf->size >= size) {
			list_del(pos);
			nd->num_buffers_in_pool--;
			return buf;
		}
	}
	return NULL;
}

/**
 * Run buffer pool garbage collection routine for the node once
 *
 * @param  pos node
 * @return     1 if a buffer has been actually released, 0 otherwise
 */
int aura_node_buffer_pool_gc_once(struct aura_node *pos)
{
	/* GC: Ditch one last buffer from pool if we have too many */
	if (pos->num_buffers_in_pool >= pos->gc_threshold) {
		struct aura_buffer *buf;
		pos->num_buffers_in_pool--;
		buf = list_entry(pos->buffer_pool.prev, struct aura_buffer, qentry);
		list_del(pos->buffer_pool.prev);
		aura_buffer_destroy(buf);
		return 1;
	}
	return 0;
}

/**
 * Run buffer pool garbage collection until no more buffers can be released
 *
 * @param  pos the node to gc for
 * @return     The number of buffers released
 */
int aura_node_buffer_pool_gc_full(struct aura_node *pos)
{
	int ret = 0;
	int tmp;
	do {
		tmp = ret;
		ret += aura_node_buffer_pool_gc_once(pos);
	} while (ret != tmp);
	return ret;
}

/**
 * Request an buffer for this node big enough to contain at least size bytes of data.
 * The data is returned in struct aura_buffer
 *
 * If the node transport overrides buffer allocation - transport-specific allocation function
 * will be called
 *
 * @param nd
 * @param size
 * @return
 */
struct aura_buffer *aura_buffer_request(struct aura_node *nd, int size)
{
	struct aura_buffer *ret = NULL;
	int act_size = size;

	act_size += nd->tr->buffer_overhead;

#ifdef AURA_USE_BUFFER_POOL
	/* Try buffer pool first */
	ret = fetch_buffer_from_pool(nd, act_size);
	if (ret)
		goto bailout; /* For the sake of readability */
#endif

	/* Fallback to alloc() */
	if (!nd->tr->allocator) {
		char *data = malloc(act_size + sizeof(struct aura_buffer));
		ret = (struct aura_buffer *)data;
		if (!ret)
			BUG(nd, "FATAL: malloc() failed");
		ret->data = &data[sizeof(*ret)];
	} else {
		ret = nd->tr->allocator->request(nd, nd->allocator_data, act_size);
		if (!ret)
			BUG(nd, "FATAL: buffer allocation by transport failed");
	}

	/* Shut up compiler warning when buffer pool is disabled */
	goto bailout;
bailout:
	ret->magic = AURA_BUFFER_MAGIC_ID;
	ret->size = act_size;
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
	/* Just put the buffer back into the pool at the very start */
#ifdef AURA_USE_BUFFER_POOL
	struct aura_node *nd = buf->owner;
	if (buf->magic != AURA_BUFFER_MAGIC_ID)
		BUG(nd,
		    "FATAL: Attempting to release a buffer with invalid magic OR double free an aura_buffer");

	list_add(&buf->qentry, &nd->buffer_pool);
	nd->num_buffers_in_pool++;
#else
	aura_buffer_destroy(buf);
#endif
}

/**
 * Force aura to immediately free the buffer, bypassing the node's buffer pool.
 * Do not call this function directly, unless you know what you are doing - use
 * aura_buffer_release() instead.
 * @param buf
 */
void aura_buffer_destroy(struct aura_buffer *buf)
{
	struct aura_node *nd = buf->owner;

	if (buf->magic != AURA_BUFFER_MAGIC_ID)
		BUG(nd,
		    "FATAL: Attempting to destroy a buffer with invalid magic OR double free an aura_buffer");
	buf->magic = 0;

	if (!nd)
		BUG(NULL, "Buffer with no owner");

	if (nd->tr->allocator)
		nd->tr->allocator->release(nd, nd->allocator_data, buf);
	else
		free(buf);
}

/**
 * Garbage-collect at most numdrop buffers from the buffer pool, if the total
 * number of buffers in the pool is greater than threshold.
 *
 * threshold == 0 and numdrop == -1 drop everything from the pool.
 *
 * @param nd
 * @param numdrop - maximum number of buffers to destroy during this iteration
 * @param threshold - maximum number of buffers to keep in the pool
 */
void aura_bufferpool_gc(struct aura_node *nd, int numdrop, int threshold)
{
	struct aura_buffer *pos, *tmp;

	/* We iterate in reverse order, since the least used buffers
	 * will naturally end up at the very end of the list
	 */
	list_for_each_entry_safe_reverse(pos, tmp, &nd->buffer_pool, qentry){
		if ((numdrop == -1)
		    || (numdrop-- && nd->num_buffers_in_pool > threshold)) {
			list_del(&pos->qentry);
			nd->num_buffers_in_pool--;
			aura_buffer_destroy(pos);
		} else {
			return; /* Done for now */
		}
	}
}

/**
 * Force-populate the bufferpool with count buffers of size size
 * Warning: If count exceeds the threshold they will start being dropped
 * by the GC.
 */
void aura_bufferpool_preheat(struct aura_node *nd, int size, int count)
{
	while (count--) {
		struct aura_buffer *buf = aura_buffer_request(nd, size);
		aura_buffer_release(buf);
		slog(0, SLOG_DEBUG, "!");
	}
}

/**
 * Manually override buffer pool gc threshold.
 * The automatic gc will start releasing buffers, one per loop once
 * the are more than threshold buffers in the free buffer pool
 *
 * @param nd The node for which we're setting the new threshold
 * @param threshold The new threshold
 */
void aura_bufferpool_set_gc_threshold(struct aura_node *nd, int threshold)
{
	nd->gc_threshold = threshold;
}

/**
 * Get the length of the data buffer without the transport overhead
 * WARNING: This may be more than specified when calling aura_buffer_request
 * due to buffer pooling. This value in no way identifies the size of the
 * actual valid data strored inside.
 *
 * @param  buf [description]
 * @return     [description]
 */
size_t aura_buffer_length(struct aura_buffer *buf)
{
	return (buf->size - buf->owner->tr->buffer_overhead);
}


/**
 * @}
 */
