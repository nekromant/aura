#include <aura/aura.h>

 /**
 * \addtogroup trapi
 * @{
 */

/**
 * Add an aura_buffer to a queue specified.
 *
 * @param queue
 * @param buf
 */
void aura_queue_buffer(struct list_head *queue, struct aura_buffer *buf)
{ 
	list_add_tail(&buf->qentry, queue);
}

/**
 * Dequeue the next buffer from a queue and
 * @param head
 * @return
 */
struct aura_buffer *aura_dequeue_buffer(struct list_head *head)
{
	struct aura_buffer *ret;
	
	if (list_empty(head))
		return NULL;
	ret = list_entry(head->next, struct aura_buffer, qentry);
	list_del(head->next);
	return ret;
}

void aura_requeue_buffer(struct list_head *list, struct aura_buffer *buf)
{
	list_add(&buf->qentry, list);	
}

/**
 * @}
 */
