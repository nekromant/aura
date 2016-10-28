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

struct aura_packet8 *aura_packed_data(char *data, size_t len);
char * aura_unpacked_data(struct aura_packet8 *packet, size_t len);

#endif
