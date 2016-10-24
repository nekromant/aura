#include <aura/aura.h>


int main() {
	slog_init(NULL, 18);

	int count = 5;
	struct aura_node *n = aura_open("dummy", NULL);
	aura_wait_status(n, AURA_STATUS_ONLINE);

	struct aura_eventloop *l = aura_eventloop_create(n);
	if (!l)
		return -1;

	aura_enable_sync_events(n, count);

	do {
		count = aura_get_pending_events(n);
		slog(0, SLOG_DEBUG, "====> %d events in queue", count);
		aura_eventloop_dispatch(l, AURA_EVTLOOP_ONCE);
	} while (count < 5);

	aura_close(n);
	aura_eventloop_destroy(l);
	return 0;
}
