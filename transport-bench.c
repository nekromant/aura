#include <aura/aura.h>
#include <aura/private.h>

static char *randstring(size_t length)
{
	static char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789,.-#'?!";
	char *randomString = NULL;

	if (length) {
		randomString = malloc(sizeof(char) * (length + 1));

		if (randomString) {
			int n;
			for (n = 0; n < length; n++) {
				int key = rand() % (int)(sizeof(charset) - 1);
				randomString[n] = charset[key];
			}

			randomString[length] = '\0';
		}
	}

	return randomString;
}

static int num_methods = 8192;
static int dummy_open(struct aura_node *node, const char *opts)
{
	num_methods = atoi(opts);
	if (!num_methods)
		BUG(node, "Bad opts for bench transort");
	slog(1, SLOG_INFO, "Opening lookup-bench transport, %d methods", num_methods);
	return 0;
}


static void dummy_populate_etable(struct aura_node *node)
{
	struct aura_export_table *etbl = aura_etable_create(node, num_methods);

	if (!etbl)
		BUG(node, "Failed to create etable");

	int i = num_methods;

	while (i--) {
		char *tmp = randstring(16);
		aura_etable_add(etbl, tmp, "1", "1");
		free(tmp);
	}

	aura_etable_activate(etbl);
}

static void dummy_close(struct aura_node *node)
{
	slog(1, SLOG_INFO, "Closing lookup-bench transport");
}

static void dummy_loop(struct aura_node *node, const struct aura_pollfds *fd)
{
	struct aura_buffer *buf;

	if (node->status != AURA_STATUS_ONLINE) {
		dummy_populate_etable(node);
		aura_set_status(node, AURA_STATUS_ONLINE);
	}

	while (1) {
		buf = aura_node_queue_read(node, NODE_QUEUE_OUTBOUND);
		if (!buf)
			break;
		aura_node_queue_write(node, NODE_QUEUE_INBOUND, buf);
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

static struct aura_transport bench = {
	.name			= "bench",
	.open			= dummy_open,
	.close			= dummy_close,
	.loop			= dummy_loop,
	.buffer_overhead	= 16,
	.buffer_offset		= 8,
	.buffer_get		= dummy_buffer_get,
	.buffer_put		= dummy_buffer_put,
};
AURA_TRANSPORT(bench);
