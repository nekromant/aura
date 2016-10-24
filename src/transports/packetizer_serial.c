#include"aura/packetizer_serial.h"

#define START_CHAR 'S'

enum packetizer_state 
{
	START_STATE = 0,
	CONT_STATE,
	LENGTH_STATE,
	CRC8_STATE,
	DATA_STATE
} packet_state;

static char 	copbuf[];
static int 	copied;

struct aura_packet8 *aura_packed_data(const char *data, size_t len)
{
	static struct aura_packet8 *res_packet;
	size_t i = 0;

	while (i < len)
		switch (packet_state){
			case START_STATE:		
				while (data[i] != START_CHAR && i < len) i++;
				packet_state = CONT_STATE;
				copied = 0;
			
				break;
			case CONT_STATE:
				res_packet->cont = data[i];
				break;
			case LENGTH_STATE:
				break;
			case CRC8_STATE:
				break;
			case DATA_STATE:
				break;
			default:
				break;
		}
	}
}


char * aura_unpacked_data(struct aura_packet8 *packet, size_t len);


