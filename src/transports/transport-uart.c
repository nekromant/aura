#include <aura/aura.h>
#include <aura/private.h>
#include <aura/packetizer.h>
#include <aura/timer.h>
#include<sys/socket.h>
#include <arpa/inet.h>
#include <sys/epoll.h>


#define CB_ARG (void *) 0xdeadf00d

struct uart_dev_info {
	int descr;
	struct sockaddr_in server;
};

static void uart_populate_etable(struct aura_node *node)
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
	uart_populate_etable(node);
	aura_set_status(node, AURA_STATUS_ONLINE);
}


#include <fcntl.h>

/** Returns true on success, or false if there was an error */
bool SetSocketBlockingEnabled(int fd, bool blocking)
{
   if (fd < 0) return false;

#ifdef WIN32
   unsigned long mode = blocking ? 0 : 1;
   return (ioctlsocket(fd, FIONBIO, &mode) == 0) ? true : false;
#else
   int flags = fcntl(fd, F_GETFL, 0);
   if (flags < 0) return false;
   flags = blocking ? (flags&~O_NONBLOCK) : (flags|O_NONBLOCK);
   return (fcntl(fd, F_SETFL, flags) == 0) ? true : false;
#endif
}

static int uart_open(struct aura_node *node, const char *opts)
{
	struct uart_dev_info *inf = calloc(1, sizeof(*inf));
	if (!inf)
		return -ENOMEM;


	slog(1, SLOG_INFO, "Opening uart transport");

	//Create socket
	inf->descr = socket(AF_INET , SOCK_STREAM, 0);
	if (inf->descr == -1)
	{
		slog(1, SLOG_ERROR, "Could not create socket");
	}
       	slog(3, SLOG_INFO, "Socket created");

    	inf->server.sin_addr.s_addr = inet_addr("127.0.0.1");
    	inf->server.sin_family = AF_INET;
    	inf->server.sin_port = htons( 8888 );

    	//Connect to remote server
    	if (connect(inf->descr , (struct sockaddr *)&inf->server , sizeof(inf->server)) < 0)
    	{
        	slog(1, SLOG_ERROR, "connect failed. Error");
        	return 1;
   	}

    	slog(3, SLOG_INFO, "Connected");

	aura_set_transportdata(node, inf);
	struct aura_timer *online = aura_timer_create(node, online_cb_fn, CB_ARG);
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 1;
	aura_timer_start(online, AURA_TIMER_FREE, &tv);
	SetSocketBlockingEnabled(inf->descr, 1);
	aura_add_pollfds(node, inf->descr, EPOLLIN | EPOLLPRI | EPOLLRDHUP);
	return 0;
}

static void uart_close(struct aura_node *node)
{
	struct uart_dev_info *inf = aura_get_transportdata(node);
	close(inf->descr);
	slog(1, SLOG_INFO, "Closing uart transport");
}

static void uart_handle_event(struct aura_node *node, enum node_event evt, const struct aura_pollfds *fd)
{
	struct aura_buffer *buf;
	struct uart_dev_info *inf = aura_get_transportdata(node);
	char str[2000] = "";
	slog(0, SLOG_DEBUG, "Node event: %d\n", evt);

	if (evt == NODE_EVENT_HAVE_OUTBOUND) {
		buf = aura_node_read(node);
		/* TODO: send/write may deliver only a few bytes here */
		int wrlen = write(inf->descr, buf->data, aura_buffer_get_length(buf));
		slog(0, SLOG_DEBUG, "%d/%d bytes written", wrlen, aura_buffer_get_length(buf));
		if (wrlen != aura_buffer_get_length(buf))
			BUG(node, "NOT IMPLEMENTED");
		aura_buffer_release(buf);
		fsync(inf->descr);
	}

	if (fd) {
		slog(0, SLOG_DEBUG, "Socket event");
		if (fd->events & EPOLLIN) {
			slog(0, SLOG_DEBUG, "We can read some shit!");
			buf = aura_buffer_request(node, 128);
			/* FixMe: We should get the id from remote side */
			buf->object = aura_etable_find(node->tbl, "echo_u16");
			int rdlen = read(inf->descr, buf->data, 128);
			buf->payload_size = rdlen;
			slog(0, SLOG_DEBUG, "Read %d bytes, assuming echo_u16", rdlen);
			aura_node_write(node, buf);
		}

		if (fd->events & EPOLLRDHUP) {
			slog(0, SLOG_DEBUG, "Disconnected from server");
			aura_set_status(node, AURA_STATUS_OFFLINE);
		}
	}


}

static struct aura_transport uart = {
	.name			  = "uart",
	.open			  = uart_open,
	.close			  = uart_close,
	.handle_event	 	  = uart_handle_event,
	.buffer_overhead  = sizeof(struct aura_packet8),
	.buffer_offset	  = sizeof(struct aura_packet8),
};
AURA_TRANSPORT(uart);
