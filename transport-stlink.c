#include <unistd.h>
#include <fcntl.h>
#include <aura/aura.h>
#include <aura/private.h>
#include <aura/packetizer.h>
#include <aura/timer.h>
#include <pthread.h>
#include <stlink.h>

struct stlink_transport_data {
	int running;
	pthread_mutex_t dmut;
	struct stlink_t* sl;
	int fd[2];
};

static void *stlink_io_thread(void *arg)
{
	struct stlink_transport_data *sl = arg;
	while (sl->running) {
		/* TODO: ... */
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
