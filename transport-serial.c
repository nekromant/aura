#include <aura/aura.h>

struct serialdata { 
	int fd; 
	uint16_t curmagic; 
};

struct __attribute__((packed)) serpacket  { 
	uint16_t magic;
	uint16_t datalen; 
	uint16_t invdatalen; 
	uint32_t crc32; 
	uint8_t   op;
	char data[];
};

/* 
static void serpacket_pack(struct aura_node *node, struct aura_buffer *buf)
{
	
}
*/
int ser_open(struct aura_node *node, va_list ap)
{
	const char *port = va_arg(ap, const char *);
	int spd  = va_arg(ap, int);
	slog(0, SLOG_INFO, "Opening serial transport: %s @ %d bps", port, spd);
	struct serialdata *sdt = calloc(1, sizeof(*sdt));
	if (sdt) 
		return -ENOMEM; 
	
	return 0;
}

void ser_close(struct aura_node *node)
{
	slog(0, SLOG_INFO, "Closing dummy transport");
}

void ser_loop(struct aura_node *node)
{
	struct aura_buffer *buf;
	struct aura_object *o;
	
	while(1) { 
		buf = aura_dequeue_buffer(&node->outbound_buffers); 
		if (!buf)
			break;
		o = buf->userdata;
		slog(0, SLOG_DEBUG, "Dequeued/requeued obj id %d (%s)", o->id, o->name);
		aura_queue_buffer(&node->inbound_buffers, buf);
	}
}

static struct aura_transport serial = { 
	.name = "serial",
	.open = ser_open,
	.close = ser_close,
	.loop  = ser_loop,
	.buffer_overhead = sizeof(struct serpacket), 
	.buffer_offset = sizeof(struct serpacket)
};
AURA_TRANSPORT(serial);
