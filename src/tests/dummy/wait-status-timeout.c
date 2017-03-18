#include <aura/aura.h>

int main() {
	slog_init(NULL, 18);

	struct aura_node *n = aura_open("dummy", "offline");
	if (!n)
		return -1;

	struct timeval tv = {
		.tv_sec = 1,
		.tv_usec = 0
	};
	int ret = aura_wait_status_timeout(n, AURA_STATUS_ONLINE, &tv);
	if (ret == AURA_STATUS_ONLINE)
		return 1;
	
	aura_close(n);

	return 0;
}
