#include <aura/aura.h>
#include <aura/private.h>
#include <aura/crc.h>

#define PACKET_START 0x7f

/*
 *
 * SEARCH -> READ_HEADER -> READ_DATA
 *
 */
enum packetizer_state {
	STATE_SEARCH_START=0,
	STATE_READ_HEADER,
	STATE_READ_DATA
};

struct __attribute__((packed)) aura_packet8  {
	uint8_t start;
	uint8_t cont;
	uint8_t datalen;
	uint8_t invdatalen;
	uint8_t crc8;
	char data[];
};

struct aura_packetizer {
	int			endian;
	uint8_t			cont;
	uint8_t			expect_cont;

	struct aura_node *	node;

	/* Packetizer callbacks */

	void			(*recvcb)(struct aura_buffer *buf, void *arg);
	void *			recvarg;

	struct aura_buffer *	(*packet_packfn)(struct aura_buffer *buf, void *arg);
	void *			packarg;

	struct aura_buffer *	(*packet_unpackfn)(struct aura_buffer *buf, void *arg);
	void *			unpackarg;

	int			state;
	struct aura_buffer *	curbuf;

	int			copied;
	struct aura_packet8	headerbuf; /* FixMe: ... */
};



int aura_packetizer_max_overhead()
{
	return sizeof(struct aura_packet8);
}

struct aura_packetizer *aura_packetizer_create(struct aura_node *node)
{
	struct aura_packetizer *pkt = malloc(sizeof(*pkt));

	if (!pkt) {
		BUG(node, "packetizer allocation failed");
		return NULL;
	}
	pkt->node = node;
	pkt->endian = -1; /* Not yet determined */
	pkt->recvcb = NULL;
	pkt->curbuf = NULL;
	pkt->cont = 0;
	pkt->copied = 0;
	return pkt;
}

void aura_packetizer_destroy(struct aura_packetizer *pkt)
{
	if (pkt->curbuf)
		aura_buffer_release(pkt->curbuf);
	free(pkt);
}

void aura_packetizer_set_receive_cb(
	struct aura_packetizer *pkt,
	void (*recvcb)(struct aura_buffer *buf, void *arg),
	void *arg)
{
	pkt->recvcb = recvcb;
	pkt->recvarg = arg;
}

void aura_packetizer_set_pack_cb(
	struct aura_packetizer *pkt,
	struct aura_buffer *(*packet_packfn)(struct aura_buffer *buf, void *arg),
	void *arg)
{
	pkt->packet_packfn = packet_packfn;
	pkt->packarg = arg;
}

void aura_packetizer_set_unpack_cb(
	struct aura_packetizer *pkt,
	struct aura_buffer *(*packet_unpackfn)(struct aura_buffer *buf, void *arg),
	void *arg)
{
	pkt->packet_unpackfn = packet_unpackfn;
	pkt->unpackarg = arg;
}

int aura_packetizer_verify_header(struct aura_packetizer *pkt, struct aura_packet8 *packet)
{
	if (packet->start != PACKET_START)
		return -1;
	if (packet->datalen != ~(packet->invdatalen))
		return -1;
	if (packet->cont != pkt->expect_cont)
		return -1;
	return 0;
}

int aura_packetizer_verify_data(struct aura_packetizer *pkt, struct aura_packet8 *packet)
{
	return !(packet->crc8 == crc8(0, (unsigned char *)packet->data, packet->datalen));
}

void aura_packetizer_encapsulate(struct aura_packetizer *pkt, struct aura_buffer *buf)
{
	struct aura_packet8 *packet = (struct aura_packet8 *)buf->data;
	int len = buf->pos - sizeof(*packet);

	if (len > 255)
		BUG(buf->owner, "Packet size is too big");

	packet->start = PACKET_START;
	packet->cont = (pkt->cont++ & 0xff);
	pkt->expect_cont = packet->cont;
	packet->datalen = len;
	packet->invdatalen = ~packet->datalen;
	packet->crc8 = crc8(0, (unsigned char *)packet->data, packet->datalen);
}

void aura_packetizer_feed(struct aura_packetizer *pkt, const char *data, size_t len)
{
	int pos = 0;

	while (pos < len) {
		switch (pkt->state) {
		case STATE_SEARCH_START:
		{
			while ((pos < len) && data[pos] != PACKET_START)
				pos++;
			if (pos < len) {
				pkt->state = STATE_READ_HEADER;
				pkt->copied = 0;
			}
			break;
		}
		case STATE_READ_HEADER:
		{
			int tocopy = min_t(int, sizeof(struct aura_packet8) - pkt->copied, len);
			struct aura_packet8 *packet = (struct aura_packet8 *)&pkt->headerbuf;
			memcpy(&pkt->headerbuf, &data[pos], tocopy);
			pkt->copied += tocopy;
			pos += tocopy;
			if (0 == (sizeof(struct aura_packet8) - pkt->copied)) {
				if (0 == aura_packetizer_verify_header(pkt, &pkt->headerbuf)) {
					pkt->state = STATE_READ_DATA;
					if (pkt->curbuf)
						BUG(pkt->node, "Internal packetizer bug");
					pkt->curbuf = aura_buffer_request(pkt->node, packet->datalen);
					if (!pkt->curbuf)
						BUG(pkt->node, "Packetizer failed to alloc buffer");
					memcpy(
						pkt->curbuf->data,
						&pkt->headerbuf,
						sizeof(pkt->headerbuf)
						);
				}
			}
			break;
		}
		case STATE_READ_DATA:
		{
			int tocopy = min_t(int, sizeof(struct aura_packet8) - pkt->copied, len);
			break;
		}
		}
	}
}
