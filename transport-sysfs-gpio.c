#include <aura/aura.h>
#include <aura/private.h>


void  sysfs_gpio_export(struct aura_node *node, struct aura_buffer *in, struct aura_buffer *out)
{
#define BUFFER_MAX 8
	char buffer[BUFFER_MAX];
	ssize_t bytes_written;
	int fd;

	fd = open("/sys/class/gpio/export", O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open export for writing!\n");
		return(-1);
	}

	bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
	write(fd, buffer, bytes_written);
	close(fd);
	return(0);
}


static int gpio_open(struct aura_node *node, va_list ap)
{
	slog(0, SLOG_INFO, "Opening sysfs/gpio transport");
	struct aura_export_table *etbl = aura_etable_create(node, 16);
	if (!etbl)
		BUG(node, "Failed to create etable");
	aura_etable_add(etbl, "gpio_write", "44", "");
	aura_etable_add(etbl, "gpio_read", "4", "4");
	aura_etable_add(etbl, "gpio_export", "4", "4");
	aura_etable_add(etbl, "gpio_wait", "4", "4");
	aura_etable_add(etbl, "gpio_watch", "4", "");
	//Change notification
	aura_etable_add(etbl, "gpio_changed", NULL, "444");
	aura_etable_activate(etbl);
	aura_set_status(node, AURA_STATUS_ONLINE);
	return 0;
}

static void gpio_close(struct aura_node *node)
{
	slog(0, SLOG_INFO, "Closing dummy transport");
}

static void gpio_loop(struct aura_node *node, const struct aura_pollfds *fd)
{
	struct aura_buffer *buf;
	struct aura_object *o;

	/* queue an event */
	buf = aura_buffer_request(node, 32);
	memset(buf->data, 12, buf->size);

	buf->userdata = aura_etable_find(node->tbl, "ping");
	aura_queue_buffer(&node->inbound_buffers, buf);

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

static struct aura_transport gpio = { 
	.name = "gpio",
	.open = gpio_open,
	.close = gpio_close,
	.loop  = gpio_loop,
};
AURA_TRANSPORT(gpio);
