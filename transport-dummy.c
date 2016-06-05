#include <aura/aura.h>
#include <aura/private.h>
#include <aura/packetizer.h>
#include <aura/timer.h>

#define CB_ARG (void *) 0xdeadf00d

static void timer_cb_fn(struct aura_node *node, struct aura_timer *tm, void *arg)
{
	if (arg != CB_ARG)
		BUG(NULL, "Unexpected CB arg: %x %x", arg, CB_ARG);

	struct aura_object *o = aura_etable_find(node->tbl, "ping");
	if (!o)
		return;
	struct aura_buffer *buf = aura_buffer_request(node, 32);
	memset(buf->data, 12, buf->size);
	buf->object = o;
	if (buf->object)
		aura_node_write(node, buf);
}

static void dummy_populate_etable(struct aura_node *node)
{
	struct aura_export_table *etbl = aura_etable_create(node, 16);

	if (!etbl)
		BUG(node, "Failed to create etable");
	aura_etable_add(etbl, "echo_str", "s128.", "s128.");
	aura_etable_add(etbl, "echo_u8", "1", "1");
	aura_etable_add(etbl, "echo_u16", "2", "2");
	aura_etable_add(etbl, "echo_i16", "7", "7");
	aura_etable_add(etbl, "echo_u32", "3", "3");
	aura_etable_add(etbl, "ping", NULL, "1");
	aura_etable_add(etbl, "echo_i32", "8", "8");
	aura_etable_add(etbl, "noargs_func", "", "");
	aura_etable_add(etbl, "echo_seq", "321", "321");
	aura_etable_add(etbl, "echo_bin", "s32.s32.", "s32.s32.");
	aura_etable_add(etbl, "echo_buf", "b", "b");
	aura_etable_add(etbl, "echo_u64", "4", "4");
	aura_etable_add(etbl, "echo_i8", "6", "6");
	aura_etable_add(etbl, "echo_i64", "9", "9");
	aura_etable_activate(etbl);
}


static void online_cb_fn(struct aura_node *node,  struct aura_timer *tm, void *arg)
{
	if (arg != CB_ARG)
		BUG(NULL, "Unexpected CB arg: %x %x", arg, CB_ARG);
	dummy_populate_etable(node);
	aura_set_status(node, AURA_STATUS_ONLINE);
}

static int dummy_open(struct aura_node *node, const char *opts)
{
	slog(1, SLOG_INFO, "Opening dummy transport");
	struct aura_timer *tm = aura_timer_create(node, timer_cb_fn, CB_ARG);
	struct aura_timer *online = aura_timer_create(node, online_cb_fn, CB_ARG);
	struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	aura_timer_start(tm, AURA_TIMER_PERIODIC, &tv);
	tv.tv_sec = 0;
	tv.tv_usec = 1;
	aura_timer_start(online, AURA_TIMER_FREE, &tv);
	return 0;
}



static void dummy_close(struct aura_node *node)
{
	slog(1, SLOG_INFO, "Closing dummy transport");
}

static void dummy_loop(struct aura_node *node, const struct aura_pollfds *fd)
{
	struct aura_buffer *buf;

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

static struct aura_transport dummy = {
	.name			  = "dummy",
	.open			  = dummy_open,
	.close			  = dummy_close,
	.loop			  = dummy_loop,
	.buffer_overhead  = sizeof(struct aura_packet8),
	.buffer_offset	  = sizeof(struct aura_packet8),
	.buffer_get		  = dummy_buffer_get,
	.buffer_put		  = dummy_buffer_put,
};
AURA_TRANSPORT(dummy);
