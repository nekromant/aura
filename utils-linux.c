#include <aura/aura.h>
#include <time.h>

uint64_t aura_platform_timestamp()
{
	struct timespec tv; 
	clock_gettime(CLOCK_MONOTONIC, &tv);
	return tv.tv_sec*1000 + tv.tv_nsec/1000000;
}
