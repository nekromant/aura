#include <aura/aura.h>


int main() {
	slog_init(NULL, 18);

	int count = 5;
	struct aura_node *n = aura_open("dummy", NULL);
	aura_wait_status(n, AURA_STATUS_ONLINE);

	struct aura_eventloop *l = aura_eventloop_create(n);
	if (!l)
		return -1;

	aura_enable_sync_events(n, 5);

	while (count < 3) {
		count = aura_get_pending_events(n);
		aura_handle_events(l);
	}

	aura_close(n);
	aura_eventloop_destroy(l);
	return 0;
}
