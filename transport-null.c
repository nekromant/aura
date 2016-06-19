#include <aura/aura.h>
#include <aura/private.h>
#include <aura/packetizer.h>
#include <aura/timer.h>

static void dummy_populate_etable(struct aura_node *node)
{
	struct aura_export_table *etbl = aura_etable_create(node, 16);

	if (!etbl)
		BUG(node, "Failed to create etable");
	aura_etable_add(etbl, "echo_u8", "1", "1");
	aura_etable_activate(etbl);
}



static int dummy_open(struct aura_node *node, const char *opts)
{
	slog(1, SLOG_INFO, "Opening dummy transport");
	return 0;
}



static void dummy_close(struct aura_node *node)
{
	slog(1, SLOG_INFO, "Closing dummy transport");
}

static void dummy_handle_event(struct aura_node *node, enum node_event evt, const struct aura_pollfds *fd)
{
	struct aura_buffer *buf;

	if (evt == NODE_EVENT_STARTED) {
		dummy_populate_etable(node);
		aura_set_status(node, AURA_STATUS_ONLINE);
	}

	while (1) {
		buf = aura_node_read(node);
		if (!buf)
			break;
		aura_node_write(node, buf);
	}
}

static void dummy_buffer_put(struct aura_buffer *dst, struct aura_buffer *buf)
{
	slog(0, SLOG_DEBUG, "dummy: serializing buf 0x%x", buf);
	uint64_t ptr = (uintptr_t)buf;
	aura_buffer_put_u64(dst, ptr);
}

static struct aura_buffer *dummy_buffer_get(struct aura_buffer *buf)
{
	struct aura_buffer *ret = (struct aura_buffer *)(uintptr_t)aura_buffer_get_u64(buf);

	slog(0, SLOG_DEBUG, "dummy: deserializing buf 0x%x", ret);
	return ret;
}

static struct aura_transport null = {
	.name			  = "null",
	.open			  = dummy_open,
	.close			  = dummy_close,
	.handle_event	  = dummy_handle_event,
	.buffer_overhead  = sizeof(struct aura_packet8),
	.buffer_offset	  = sizeof(struct aura_packet8),
	.buffer_get		  = dummy_buffer_get,
	.buffer_put		  = dummy_buffer_put,
};
AURA_TRANSPORT(null);
