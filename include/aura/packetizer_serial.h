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

struct aura_packet8 *aura_packed_data(const char *data, size_t len);
char * aura_unpacked_data(struct aura_packet8 *packet, size_t len);

#endif
