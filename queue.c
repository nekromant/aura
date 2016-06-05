#include <aura/aura.h>
#include <aura/private.h>
#include <aura/eventloop.h>
/**
 * \addtogroup trapi
 * @{
 */

/**
 * Add an aura_buffer to a queue.
 * This functions sets buffer's internal data pointer
 * to the beginning of serialized data by calling aura_buffer_rewind() internally
 *
 * @param queue
 * @param buf
 */
void aura_queue_buffer(struct list_head *queue, struct aura_buffer *buf)
{
	list_add_tail(&buf->qentry, queue);
	aura_buffer_rewind(buf);
}


/**
 * Dequeue the next buffer from a queue and
 * @param head
 * @return
 */
struct aura_buffer *aura_peek_buffer(struct list_head *head)
{
	struct aura_buffer *ret;

	if (list_empty(head))
		return NULL;
	ret = list_entry(head->next, struct aura_buffer, qentry);
	return ret;
}

/**
 * Dequeue the next buffer from a queue and return it.
 * This functions sets buffer's internal data pointer
 * to the beginning of serialized data by calling aura_buffer_rewind() internally
 * @param head
 * @return
 */
struct aura_buffer *aura_dequeue_buffer(struct list_head *head)
{
	struct aura_buffer *ret;

	ret = aura_peek_buffer(head);
	if (ret) {
		list_del(head->next);
		aura_buffer_rewind(ret);
	}
	return ret;
}

/**
 * Put the buffer back to the start of the queue.
 * @param head
 * @return
 */
void aura_requeue_buffer(struct list_head *list, struct aura_buffer *buf)
{
	list_add(&buf->qentry, list);
}



struct aura_buffer *aura_node_read(struct aura_node *node)
{
	struct aura_buffer *ret;

	ret = aura_peek_buffer(&node->outbound_buffers);
	if (ret) {
		list_del(node->outbound_buffers.next);
		aura_buffer_rewind(ret);
	}
	return ret;
}

int aura_node_outbound_avail(struct aura_node *node)
{
	return list_empty(&node->outbound_buffers);
}



/**
 * @}
 */
