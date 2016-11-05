#ifndef PACKETIZER_SERIAL_H
#define PACKETIZER_SERIAL_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

//#define START_CHAR 'S'

struct __attribute__((packed)) aura_packet8 {
	uint8_t start;
	uint8_t cont;
	uint8_t datalen;
	uint8_t invdatalen;
	uint8_t crc8;
	char data[];
};


struct aura_packetizer {
	int 			endian;
	uint8_t			cout;
	uint8_t			expect_cont;
	
	struct aura_node *	node;

	/* Packetizer callbacks */

	void			(*recvcb)(struct aura_buffer *buf, void *arg);
	void *			recvarg;

	int 			state;
	struct aura_buffer *	curbuf;

	uint8_t 		copied;
	struct aura_packet8	headerbuf; /* FixMe: ... */
};

struct aura_packetizer *aura_packetizer_create(struct aura_node *node);

void aura_packetizer_destroy(struct aura_packetizer *pkt);

void aura_packetizer_set_receive_cb(
	struct aura_packetizer *pkt,
	void (*recvcb)(struct aura_buffer *buf, void *arg),
	void *arg);

int aura_packetizer_verify_header(struct aura_packetizer *pkt, struct aura_packet8 *packet);
int aura_packetizer_verify_data(struct aura_packetizer *pkt, struct aura_packet8 *packet);

int aura_packetizer_feed_once(struct aura_packetizer *pkt, const char *data, size_t len);
void aura_packetizer_feed(struct aura_packetizer *pkt, const char *data, size_t len);

struct aura_packet8 *aura_packed_data(char *data, size_t len);
char * aura_unpacked_data(struct aura_packet8 *packet, size_t len);

#endif
