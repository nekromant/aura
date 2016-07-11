#include <unistd.h>
#include <fcntl.h>
#include <aura/aura.h>
#include <aura/private.h>
#include <aura/packetizer.h>
#include <aura/timer.h>
#include <pthread.h>
#include <stlink.h>
#include <sys/epoll.h>

struct stlink_transport_data {
	int running;
	pthread_mutex_t dmut;
	stlink_t* sl;
	char *serial;
	int fd[2];

	struct aura_buffer *txbuf;
	struct aura_buffer *rxbuf;

	struct aura_export_table *etbl;
};

struct export_object {
	uint32_t type;
	uint32_t name;
	uint32_t arg_format;
	uint32_t ret_format;
};

struct export_table {
	uint32_t count;
	uint32_t objects[]; /* pointers! */
};

struct circ_buf {
	uint32_t head;
	uint32_t tail;
	uint32_t data_ptr;
};

struct export_header {
	uint32_t magic;
	uint32_t export_tbl_addr;
	struct circ_buf h2s;
	struct circ_buf s2h;
};

struct message {
	uint32_t magic;
};

static stlink_t *stlink_open_first(char *serial)
{
    stlink_t* sl = NULL;
    sl = stlink_v1_open(0, 1);
    if (sl == NULL)
        sl = stlink_open_usb(0, 1, serial);
    return sl;
}

static int stlink_prepare(struct stlink_transport_data *stl)
{
	if (!stl->sl) {
		stl->sl = stlink_open_first(stl->serial);
		if (!stl->sl) {
			sleep(1);
			return 0;
		}
	}
	return 1;
}

static void *stlink_io_thread(void *arg)
{
	struct stlink_transport_data *stl = arg;
	while (stl->running) {
		/* Make sure we're conected */
		if (!stlink_prepare(stl))
			continue;

	}
	return NULL;
}

static int aura_stlink_open(struct aura_node *node, const char *opts)
{
	struct stlink_transport_data *td = calloc(1, sizeof(*td));
	if (!td)
		return -ENOMEM;

	int ret = pipe2(td->fd, O_NONBLOCK);
	if (ret !=0)
		goto err_free_mem;
	aura_add_pollfds(node, td->fd[0], EPOLLIN);

	aura_set_userdata(node, td);
err_free_mem:
	free(td);
	return -ENOMEM;
}



static void aura_stlink_close(struct aura_node *node)
{

}

static void aura_stlink_handle_event(struct aura_node *node, enum node_event evt, const struct aura_pollfds *fd)
{
	struct aura_buffer *buf;

	while (1) {
		buf = aura_node_read(node);
		if (!buf)
			break;
		aura_node_write(node, buf);
	}
}

static struct aura_transport stlink = {
	.name			  = "stlink",
	.open			  = aura_stlink_open,
	.close			  = aura_stlink_close,
	.handle_event	  = aura_stlink_handle_event,
	.buffer_overhead  = 0,
	.buffer_offset	  = 0,
};
AURA_TRANSPORT(stlink);
