#include <aura/aura.h>
#include <aura/private.h>
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





struct aura_buffer *aura_node_queue_read(struct aura_node *node,
	enum node_buffer_queue queue_type)
{
	struct aura_buffer *ret;
	struct list_head *queue;
	if (queue_type == NODE_QUEUE_INBOUND) {
		queue = &node->inbound_buffers;
	} else {
		queue = &node->outbound_buffers;
	}

	ret = aura_peek_buffer(queue);
	if (ret) {
		list_del(queue->next);
		aura_buffer_rewind(ret);
	}
	return ret;

}

void aura_node_queue_write(struct aura_node *node,
		     enum node_buffer_queue queue_type, struct aura_buffer *buf)
{
	struct list_head *queue;
	enum node_event evt;
	if (queue_type == NODE_QUEUE_INBOUND) {
		queue = &node->inbound_buffers;
		evt = NODE_EVENT_HAVE_INBOUND;
	} else {
		queue = &node->outbound_buffers;
		evt = NODE_EVENT_HAVE_INBOUND;
	}

	int is_first = (!list_empty(queue));
	list_add_tail(&buf->qentry, queue);
	aura_buffer_rewind(buf);
	if (is_first) {
		struct aura_eventloop *loop;
		loop = aura_node_eventloop_get(node);
		if (!loop) {
				slog(0, SLOG_WARN, "queue: No eventloop, ignoring event");
				return;
		}
		aura_eventloop_report_event(loop, evt, NULL);
	}
}


/**
 * @}
 */
