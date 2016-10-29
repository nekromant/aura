#include "packetizer_serial.h"
#include "stdio.h"
#include "my_debug.h"

int main()
{
	printf("I'm here!!!\n");
	
	struct aura_packet8 * my_packed  = aura_packed_data("ffS1224567890", 11);
	printf("my_packed->data %s\n", my_packed->data);

	char * data = aura_unpacked_data(my_packed, my_packed->datalen);
	printf("data %s\n", data);
}
