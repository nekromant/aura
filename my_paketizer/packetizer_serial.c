#include "packetizer_serial.h"
#include "my_debug.h"

#define START_CHAR 'S'

enum packetizer_state 
{
	START_STATE = 0,
	CONT_STATE,
	DATALEN_STATE,
	INVDATALEN_STATE,
	CRC8_STATE,
	DATA_STATE
} packet_state;

struct aura_packet8 *aura_packed_data(char *data, size_t len)
{
	printf("aura_packed_data\n");
	static struct aura_packet8 *res_packet;
	static uint8_t datalen = 0;
	res_packet = (struct aura_packet8 *) malloc(sizeof(struct aura_packet8));
	size_t i = 0;
	
	for (i = 0; i < len; i++)
		switch (packet_state){
			case START_STATE:		
				printf("Start state %d %c \n", (int) i, data[i]);
				if (data[i] == START_CHAR) {
					packet_state = CONT_STATE;
				}
				break;
			case CONT_STATE:
				printf("Cont staten %d %c \n", (int) i, data[i]);
				res_packet->cont = data[i];
				packet_state = DATALEN_STATE;
				break;
			case DATALEN_STATE:
				printf("datalen staten %d %c \n", (int) i, data[i]);
				res_packet->datalen = data[i];
				packet_state = INVDATALEN_STATE;
				break;
			case INVDATALEN_STATE:
				printf("invdatalen staten %d %c \n", (int) i, data[i]);
				res_packet->invdatalen = data[i];
				packet_state = CRC8_STATE;
				break;
			case CRC8_STATE:
				printf("CRC8 staten %d %c \n", (int) i, data[i]);
				res_packet->crc8 = data[i];
				packet_state = DATA_STATE;
				break;
			case DATA_STATE:
				printf("data staten %d %c \n", (int) i, data[i]);
				res_packet->data[datalen] = data[i];
				datalen++;
				if (datalen == res_packet->datalen) {
					datalen = 0;
					packet_state = START_STATE;
				}
				break;
			default:
				break;
		}
	return res_packet;
}


//char * aura_unpacked_data(struct aura_packet8 *packet, size_t len);


