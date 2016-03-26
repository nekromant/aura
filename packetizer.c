#include <aura/aura.h>
#include <aura/private.h>
#include <aura/crc.h>
#include <aura/packetizer.h>


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

int aura_packetizer_verify_header(struct aura_packetizer *pkt, struct aura_packet8 *packet)
{
	if (packet->start != PACKET_START)
		return -1;
	if (packet->datalen != ((~packet->invdatalen) & 0xff))
		return -2;
	if (packet->cont != pkt->expect_cont)
		return -3;
	return 0;
}

int aura_packetizer_verify_data(struct aura_packetizer *pkt, struct aura_packet8 *packet)
{
	uint8_t crc = crc8(0, (unsigned char *)packet->data, packet->datalen);
	return !(packet->crc8 == crc);
}

void aura_packetizer_encapsulate(struct aura_packetizer *	pkt,
				 struct aura_packet8 *		packet,
				 size_t				len)
{
	if (len > 255)
		BUG(pkt->node, "Packet is too big");

	packet->start = PACKET_START;
	packet->cont = (pkt->cont++ & 0xff);
	pkt->expect_cont = packet->cont;
	packet->datalen = len;
	packet->invdatalen = ~packet->datalen;
	packet->crc8 = crc8(0, (unsigned char *)packet->data, packet->datalen);
}

void aura_packetizer_reset(struct aura_packetizer *pkt)
{
	pkt->state = STATE_SEARCH_START;
	pkt->copied = 0;
	if (pkt->curbuf) {
		aura_buffer_release(pkt->curbuf);
		pkt->curbuf = NULL;
	}
}

static void packetizer_dispatch_packet(struct aura_packetizer *pkt)
{
	slog(4, SLOG_DEBUG, "packetizer: dispatching packet");
	aura_buffer_rewind(pkt->curbuf);
	if (pkt->recvcb)
		pkt->recvcb(pkt->curbuf, pkt->recvarg);
	else /* No callback? Free the buffer */
		aura_buffer_release(pkt->curbuf);
	pkt->curbuf = NULL;
	aura_packetizer_reset(pkt);
}

/**
 * Feeds at most one packet into the packetizer
 * @param  pkt  packetizer instance
 * @param  data data to feed
 * @param  len  the length of the data
 * @return      The number of bytes consumed
 */
int aura_packetizer_feed_once(struct aura_packetizer *pkt, const char *data, size_t len)
{
	int pos = 0;
	int ret;
	struct aura_packet8 *hdr = &pkt->headerbuf;
	switch (pkt->state) {
	case STATE_SEARCH_START:
		while ((pos < len) && data[pos] != PACKET_START)
			pos++;
		if (pos < len) {
			pkt->state = STATE_READ_HEADER;
			pkt->copied = 0;
			slog(4, SLOG_DEBUG, "packetizer: Found start at %d", pos);
		}
		break;
	case STATE_READ_HEADER:
	{
		int tocopy = min_t(int, sizeof(struct aura_packet8) - pkt->copied, len);
		char *dest = (char *)&pkt->headerbuf;
		memmove(&dest[pkt->copied], &data[pos], tocopy);
		pkt->copied += tocopy;
		pos += tocopy;
		if ((sizeof(struct aura_packet8) == pkt->copied)) {
			ret = aura_packetizer_verify_header(pkt, &pkt->headerbuf);
			if (0 == ret) {
				pkt->state = STATE_READ_DATA;
				pkt->copied = 0;
				if (pkt->curbuf)
					BUG(pkt->node, "Internal packetizer bug");
				pkt->curbuf = aura_buffer_request(pkt->node, hdr->datalen);
				if (!pkt->curbuf)
					BUG(pkt->node, "Packetizer failed to alloc buffer");
				memcpy(
					pkt->curbuf->data,
					&pkt->headerbuf,
					sizeof(pkt->headerbuf)
					);
			} else {
				aura_packetizer_reset(pkt);
				aura_packetizer_feed(pkt, &dest[1],
									sizeof(struct aura_packet8) - 1);
			}
		}
		break;
	}
	case STATE_READ_DATA:
	{
		int tocopy = min_t(int, hdr->datalen - pkt->copied, len);
		aura_buffer_put_bin(pkt->curbuf, &data[pos], tocopy);
		pos += tocopy;
		pkt->copied += tocopy;
		if (pkt->copied == hdr->datalen) {
			aura_hexdump("out", pkt->curbuf->data, pkt->curbuf->pos);;
			ret = aura_packetizer_verify_data(pkt,
							     (struct aura_packet8 *) pkt->curbuf->data);
			if (ret == 0) {
				packetizer_dispatch_packet(pkt);
			} else {
				struct aura_buffer *tmp = pkt->curbuf;
				int torefeed = pkt->copied;
				pkt->curbuf = NULL;
				aura_buffer_rewind(tmp);
				aura_packetizer_reset(pkt);
				aura_packetizer_feed(pkt, aura_buffer_get_bin(tmp, torefeed),
									torefeed);
				aura_buffer_release(tmp);
			}
		}
		break;
	}
	}
	return pos;
}
/**
 * Feed all of the buffer into the packetizer. This may result in no to several
 * callbacks from the packetizer core
 * @param pkt  packetizer instance
 * @param data buffer to feed into the packetizer
 * @param len  the length of the buffer in bytes
 */
void aura_packetizer_feed(struct aura_packetizer *pkt, const char *data, size_t len)
{
	int pos=0;
	while (pos < len)
		pos+= aura_packetizer_feed_once(pkt, &data[pos], len-pos);
}
