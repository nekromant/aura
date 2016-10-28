#include "packetizer_serial.h"
#include "stdio.h"
#include "my_debug.h"

int main()
{
	printf("I'm here!!!\n");
	
	struct aura_packet8 * my_packed  = aura_packed_data("S1234567890", 11);
	struct aura_packet8 * my_packed1 = aura_packed_data("123S1234567890", 11);
	struct aura_packet8 * my_packed2 = aura_packed_data("123S45678S90", 11);
}
