#include <aura/aura.h>
#include <inttypes.h>
#include <unistd.h> 

int main()
{
	while (1) { 
		slog(0, SLOG_LIVE, "timestamp: %" PRIu64 "\n", aura_platform_timestamp());
		usleep(300);
	}
}
