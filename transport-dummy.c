#include <aura/aura.h>
#include <aura/private.h>

int dummy_open(struct aura_node *node, va_list ap)
{
	slog(0, SLOG_INFO, "Opening dummy transport");
	struct aura_export_table *etbl = aura_etable_create(node, 16);
	if (!etbl)
		BUG(node, "Failed to create etable");
	aura_etable_add(etbl, "echo_str", "s128.", "s128.");
	aura_etable_add(etbl, "echo_u8", "1", "1");
	aura_etable_add(etbl, "echo_u16", "2", "2");
	aura_etable_add(etbl, "echo_i16", "7", "7");
	aura_etable_activate(etbl);
	aura_set_status(node, AURA_STATUS_ONLINE);
	return 0;
}

void dummy_close(struct aura_node *node)
{
	slog(0, SLOG_INFO, "Closing dummy transport");
}

void dummy_loop(struct aura_node *node, const struct aura_pollfds *fd)
{
	struct aura_buffer *buf;
	struct aura_object *o;
	
	while(1) { 
		buf = aura_dequeue_buffer(&node->outbound_buffers); 
		if (!buf)
			break;
		o = buf->userdata;
		slog(0, SLOG_DEBUG, "Dequeued/requeued obj id %d (%s)", o->id, o->name);
		aura_queue_buffer(&node->inbound_buffers, buf);
		aura_eventloop_interrupt(aura_eventloop_get_data(node));
	}
}

static struct aura_transport dummy = { 
	.name = "dummy",
	.open = dummy_open,
	.close = dummy_close,
	.loop  = dummy_loop,
	.buffer_overhead = 16, 
	.buffer_offset = 8
};
AURA_TRANSPORT(dummy);
