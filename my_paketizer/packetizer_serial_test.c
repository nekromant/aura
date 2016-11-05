#include "packetizer_serial.h"
#include "my_debug.h"

int main()
{
	printf("I'm here!!!\n");
	
	struct aura_packetizer * my_packetizer = aura_packetizer_create(NULL);

	aura_packetizer_destroy(my_packetizer);

	struct aura_packet8 * my_packet = malloc(sizeof(struct aura_packet8));
	my_packet = aura_packed_data("ffS1224567890", 11);
	my_packet = aura_packed_data("dsldksl", 7);
	printf("my_packed->data %s\n", my_packet->data);

	char * data = aura_unpacked_data(my_packet, my_packet->datalen);
	printf("data %s\n", data);

	return 0;
}
