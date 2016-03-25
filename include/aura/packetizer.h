#ifndef PACKETIZER_H
#define PACKETIZER_H
#include <stdint.h>
#include <stdio.h>

#define PACKET_START 0x7f

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

int aura_packetizer_max_overhead();

struct aura_packetizer *aura_packetizer_create(struct aura_node *node);

void aura_packetizer_destroy(struct aura_packetizer *pkt);

void aura_packetizer_set_receive_cb(
	struct aura_packetizer *pkt,
	void (*recvcb)(struct aura_buffer *buf, void *arg),
	void *arg);

int aura_packetizer_verify_header(struct aura_packetizer *pkt, struct aura_packet8 *packet);
int aura_packetizer_verify_data(struct aura_packetizer *pkt, struct aura_packet8 *packet);
void aura_packetizer_encapsulate(struct aura_packetizer *pkt,
								struct aura_packet8 *packet,
								size_t len);
int aura_packetizer_feed_once(struct aura_packetizer *pkt, const char *data, size_t len);
void aura_packetizer_feed(struct aura_packetizer *pkt, const char *data, size_t len);

#endif
