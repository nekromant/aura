#include <aura/packetizer_serial.h>
#include <aura/aura.h>
//#include "stdio.h"

int main()
{
	printf("I'm here!!!\n");
	
	struct aura_packet10 * my_packed;	
	aura_unpacked_data(my_packed, 23);
}
