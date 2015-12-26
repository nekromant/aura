#include <aura/aura.h>
#include <aura/private.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


static int file_get(const char *path, char *dbuf, int dlen)
{
	int ret; 
	int fd = open(path, O_RDONLY);
	if (-1 == fd) {
		slog(0, SLOG_ERROR, "sysfsgpio: Failed to open %s for reading", path);
		return -1;
	}
	ret = read(fd, dbuf, dlen);
	dbuf[ret]=0x0;
	close(fd);
	return ret;
}

static int file_put(const char *path, const char *dbuf)
{
	int ret; 
	int fd = open(path, O_WRONLY);
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
		slog(0, SLOG_WARN, "sysfsgpio: GPIO %d already exported", gpio);
		return 0;
	}

	sprintf(tmp, "%d", gpio);
	if (0 < file_put("/sys/class/gpio/export", tmp)) 
		return 0;
	return -1;
}

static int check_exported(int gpio)
{
	int ret; 
	char tmp[256];
	sprintf(tmp, "/sys/class/gpio/gpio%d", gpio);
	if (0 != access(tmp, F_OK)) { 
		slog(0, SLOG_WARN, "sysfsgpio: GPIO %d not exported, exporting", gpio);
		if ((ret = gpio_export(gpio)))
			return ret;
	}
	return 0;
	
}

static int gpio_write(int gpio, int value)
{
	int ret;
	char tmp[256];

	if ((ret = check_exported(gpio)))
		return ret; 

	sprintf(tmp, "/sys/class/gpio%d/value", gpio);
	if (0 < file_put(tmp, value ? "1" : "0")) 
		return 0;

	return -EIO;
}

static int gpio_read(int gpio, int *value)
{
	int ret;
	char tmp[256];

	if ((ret = check_exported(gpio)))
		return ret; 

	sprintf(tmp, "/sys/class/gpio%d/value", gpio);
	if (0 < file_get(tmp, tmp, 256)) 
		return 0;

	*value = atoi(tmp);

	return -EIO;
}

static int gpio_in(int gpio)
{
	int ret;
	char tmp[256];

	if ((ret = check_exported(gpio)))
		return ret; 

	sprintf(tmp, "/sys/class/gpio%d/direction", gpio);
	if (0 < file_put(tmp, "in")) 
		return 0;

	return -EIO;
}

static int gpio_out(int gpio)
{
	int ret;
	char tmp[256];

	if ((ret = check_exported(gpio)))
		return ret; 

	sprintf(tmp, "/sys/class/gpio%d/direction", gpio);
	if (0 < file_put(tmp, "out")) 
		return 0;

	return -EIO;
}

static int gpio_open(struct aura_node *node, const char *opts)
{
	slog(0, SLOG_INFO, "Opening sysfs/gpio transport");
	struct aura_export_table *etbl = aura_etable_create(node, 16);
	if (!etbl)
		BUG(node, "Failed to create etable");
	aura_etable_add(etbl, "write", "33", "");
	aura_etable_add(etbl, "read", "3", "3");
	aura_etable_add(etbl, "out", "3", "");
	aura_etable_add(etbl, "in", "3", "");
	aura_etable_add(etbl, "export", "3", "3");
	aura_etable_add(etbl, "watch", "3", "");
	//Change notification
	aura_etable_add(etbl, "gpio_changed", NULL, "333");
	aura_etable_activate(etbl);
	aura_set_status(node, AURA_STATUS_ONLINE);
	slog(0, SLOG_INFO, "Opened sysfs/gpio transport");
	return 0;
}

static void gpio_close(struct aura_node *node)
{
	slog(0, SLOG_INFO, "Closing dummy transport");
}

#define OPCODE(_name) (strcmp(o->name, _name)==0)

static void handle_outbound(struct aura_node *node, struct aura_object *o, struct aura_buffer *buf)
{
	int ret = -EIO;
	if (OPCODE("export")) {
		int gpio = aura_buffer_get_u32(buf);
		slog(4, SLOG_DEBUG, "gpio: export %d", gpio);
		ret = gpio_export(gpio);
	} else if (OPCODE("write")) {
		int gpio = aura_buffer_get_u32(buf);
		int value = aura_buffer_get_u32(buf);
		slog(4, SLOG_DEBUG, "gpio: write gpio %d value %d", gpio, value);
		ret = gpio_write(gpio, value);
	} else if (OPCODE("in")) {
		int gpio = aura_buffer_get_u32(buf);
		ret = gpio_in(gpio);
	} else if (OPCODE("out")) {
		int gpio = aura_buffer_get_u32(buf);
		ret = gpio_out(gpio);		
	} else if (OPCODE("read")) {
		int gpio = aura_buffer_get_u32(buf);
		ret = gpio_read(gpio, &gpio);
		aura_buffer_rewind(buf);
		aura_buffer_put_u32(buf, gpio);
	}
	slog(0, SLOG_DEBUG, "gpio ret = %d", ret);
	if (ret) {
		aura_call_fail(node, o);
		return;
	}
	aura_queue_buffer(&node->inbound_buffers, buf);
}

static void gpio_loop(struct aura_node *node, const struct aura_pollfds *fd)
{
	struct aura_buffer *buf;
	struct aura_object *o;

	while(1) { 
		buf = aura_dequeue_buffer(&node->outbound_buffers); 
		if (!buf)
			break;
		o = buf->object;
		handle_outbound(node, o, buf);
		aura_buffer_release(buf);
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
