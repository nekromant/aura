#include <aura/aura.h>
#include <aura/private.h>
#include <aura/packetizer.h>
#include <aura/timer.h>
#include<sys/socket.h> 
#include<arpa/inet.h> 
 

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


	while (1) {
		buf = aura_node_read(node);
	
		if (!buf)
			break;

		slog(0, SLOG_DEBUG, "uart: serializing buf data %x len 0x%x", buf->data, aura_buffer_get_length(buf));

        	if (send(inf->descr, buf->data, aura_buffer_get_length(buf), MSG_DONTWAIT) < 0)
        	{
            		slog(1, SLOG_ERROR, "Send failed");
        	}

	        slog(0, SLOG_DEBUG, "Send done \n");
		
		int ret = read(inf->descr, str, 2);
		slog(1, SLOG_INFO, "read %d bytes", ret);

		if(recv(inf->descr, str, 2 , MSG_DONTWAIT) < 0)
		{
			slog(1, SLOG_ERROR, "read failed");
		}

		slog(1, SLOG_INFO, str);

		if (buf->object != NULL)
			printf("s1='%d'\n", (uint16_t) &buf->data[0]);

		aura_node_write(node, buf);
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
