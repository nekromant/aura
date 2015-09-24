#include <aura/aura.h>
#include <aura/private.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>



static int file_get(const char *path, const char *dbuf)
{
	int ret; 
	fd = open(path, O_RDONLY);
	if (-1 == fd) {
		slog(0, SLOG_ERROR, "sysfsgpio: Failed to open %s for reading", path);
		return -1;
	}
	ret = read(fd, dbuf, strlen(dbuf));
	close(fd);
	return ret;
}

static int file_put(const char *path, const char *dbuf)
{
	int ret; 
	fd = open(path, O_WRONLY);
	if (-1 == fd) {
		slog(0, SLOG_ERROR, "sysfsgpio: Failed to open %s for writing", path);
		return -1;
	}
	ret = write(fd, dbuf, strlen(dbuf));
	close(fd);
	return ret;
}

static int gpio_export(int gpio)
{
	char tmp[128];
	sprintf(tmp, "/sys/class/gpio/gpio%d", gpio);
	if (0 == access(tmp, F_OK)) { 
		slog(0, SLOG_WARN, "sysfsgpio: GPIO%d already exported");
		return 0;
	}
	sprintf(tmp, "%d", gpio);
	if (0 < file_put("/sys/class/gpio/export", tmp)) 
		return 0;
	return -1;
}

static int gpio_write(int gpio, int value)
{
	char tmp[256];
	sprintf(tmp, "/sys/class/gpio/gpio%d/direction", gpio);
	if (0 != access(tmp, F_OK)) { 
		slog(0, SLOG_WARN, "sysfsgpio: GPIO%d not exported, exporting");
		if (gpio_export(gpio)
		return 0;
	}
	sprintf(tmp, "%d", gpio);
	if (0 < file_put("/sys/class/gpio/export", tmp)) 
		return 0;
	return -1;
}


void  sysfs_gpio_export(struct aura_node *node, struct aura_buffer *in, struct aura_buffer *out)
{
#define BUFFER_MAX 8
	int pin = 0;
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


static int gpio_open(struct aura_node *node, const char *opts)
{
	slog(0, SLOG_INFO, "Opening sysfs/gpio transport");
	struct aura_export_table *etbl = aura_etable_create(node, 16);
	if (!etbl)
		BUG(node, "Failed to create etable");
	aura_etable_add(etbl, "gpio_write", "33", "");
	aura_etable_add(etbl, "gpio_read", "3", "4");
	aura_etable_add(etbl, "gpio_export", "3", "3");
	aura_etable_add(etbl, "gpio_watch", "3", "");
	//Change notification
	aura_etable_add(etbl, "gpio_changed", NULL, "333");
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
