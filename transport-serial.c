#include <aura/aura.h>
#include <aura/private.h>

struct serialdata { 
	int fd; 
	uint16_t curmagic; 
	uint64_t lastping;
};

struct __attribute__((packed)) serpacket  { 
	uint8_t  start;
	uint16_t datalen; 
	uint16_t invdatalen; 
	uint32_t crc32; 
	char data[];
};

enum serpacket_type { 
	PACKET_HELLO,
	PACKET_INFO,
	PACKET_PING, 
	PACKET_CALL,
};


struct __attribute__((packed)) serpacket_data {
	uint8_t type;	
	union {
		
	} data;
};

/* 
static void serpacket_pack(struct aura_node *node, struct aura_buffer *buf)
{
	
}
*/
static int ser_open(struct aura_node *node, const char *opts)
{
	slog(0, SLOG_INFO, "Opening serial transport: %s", opts);
	struct serialdata *sdt = calloc(1, sizeof(*sdt));
	if (sdt) 
		return -ENOMEM; 
	
	return 0;
}

static void ser_close(struct aura_node *node)
{
	slog(0, SLOG_INFO, "Closing dummy transport");
}

static void ser_loop(struct aura_node *node, const struct aura_pollfds *fd)
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
