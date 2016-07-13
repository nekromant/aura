#include <unistd.h>
#include <fcntl.h>
#include <aura/aura.h>
#include <aura/private.h>
#include <aura/packetizer.h>
#include <aura/timer.h>
#include <pthread.h>
#include <stlink.h>
#include <poll.h>

//#define AURA_STLINK_MAGIC 0xB00BD00D
#define AURA_STLINK_MAGIC 0xDEADF00D
#define READ_UINT32_LE(buf)  ((uint32_t) (  buf[0]         \
                                          | buf[1] <<  8   \
                                          | buf[2] << 16   \
                                          | buf[3] << 24))

struct stlink_transport_data {
	char *serial;
	struct aura_node *node;
	int running;
	int fd[2];

	pthread_t iothread;
	pthread_mutex_t dmut;
	stlink_t* sl;

	struct aura_buffer *txbuf;
	bool tx_complete;
	struct aura_export_table *etbl;
	struct aura_buffer *rxbuf;
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
	uint32_t magic; /* AURA_STLINK_MAGIC */
	uint32_t export_tbl_addr; /* Pointer to flash */
	struct circ_buf h2s;
	struct circ_buf s2h;
};

struct message {
	uint32_t magic;
};


static void notify_aura_thread(struct stlink_transport_data *stl)
{
	char tmp = 123;
	if (sizeof(tmp) != write(stl->fd[1], &tmp, sizeof(tmp)))
		BUG(stl->node, "IPC error");
}

static stlink_t *stlink_open_first(char *serial)
{
    stlink_t* sl = NULL;
    sl = stlink_v1_open(0, 1);
    if (sl == NULL)
        sl = stlink_open_usb(0, 1, serial);
    return sl;
}

static int aura_stlink_connect(struct stlink_transport_data *stl, uint32_t addr)
{
	slog(4, SLOG_DEBUG, "stlink: Trying magic at 0x%x", addr);
	return 1;
}

static void stlink_panic(struct stlink_transport_data *stl)
{
	stlink_close(stl->sl);
	stl->sl = NULL;
}

static int scan_for_magic(struct stlink_transport_data *stl)
{
	 static const uint32_t sram_base = 0x20000000;
	 uint32_t off;
	 for (off = 0; off < stl->sl->sram_size; off += 4) {
	        stlink_read_mem32(stl->sl, sram_base + off, 4);
	        if ((AURA_STLINK_MAGIC == READ_UINT32_LE(stl->sl->q_buf)) &&
				aura_stlink_connect(stl, sram_base + off))
					return 1;
		}
	return 0;
}

static int stlink_prepare(struct stlink_transport_data *stl)
{
	if (!stl->sl) {
		stl->sl = stlink_open_first(stl->serial);
		if (!stl->sl) {
			sleep(1);
			return 0;
		}
	    slog(4, SLOG_DEBUG, "stlink device opened!");
		if (!scan_for_magic(stl)) {
			stlink_panic(stl);
			slog(0, SLOG_DEBUG, "stlink: No valid aura rpc structure found");
			return 0;
		}
		slog(4, SLOG_DEBUG, "NOTIFY");
		notify_aura_thread(stl);
	}
	return 1;
}

static void *stlink_io_thread(void *arg)
{
	slog(4, SLOG_DEBUG, "stlink io thread started");
	struct stlink_transport_data *stl = arg;
	while (stl->running) {
		/* Make sure we're conected */
		if (!stlink_prepare(stl))
			continue;
	}
	slog(4, SLOG_DEBUG, "stlink io thread shutting down");
	return NULL;
}

static int aura_stlink_open(struct aura_node *node, const char *opts)
{
	struct stlink_transport_data *td = calloc(1, sizeof(*td));
	if (!td)
		return -ENOMEM;

	int ret = pipe(td->fd);
	if (ret !=0)
		goto err_free_mem;

	int saved_flags = fcntl(td->fd[0], F_GETFL);
	fcntl(td->fd[0], F_SETFL, saved_flags | O_NONBLOCK);
	uint32_t tmp=0;
	aura_add_pollfds(node, td->fd[0], POLLIN | POLLPRI);
	aura_set_transportdata(node, td);
	td->node = node;
	td->running = 1;
	ret = pthread_create (&td->iothread, NULL, stlink_io_thread, td);
	if (ret)
		goto err_close_pipe;

	return 0;
err_close_pipe:
	close(td->fd[0]);
	close(td->fd[1]);
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
	struct stlink_transport_data *stl = aura_get_transportdata(node);
	slog(0, SLOG_DEBUG, "event %d fd %x\n", evt, fd);
	if (fd) {
		printf("!!!!!!!!!!!!!!\n");
		char tmp;
		read(stl->fd[0], &tmp, 1);
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
