#include <aura/aura.h>


int main() {
	slog_init(NULL, 18);

	int count = 5;
	int ret; 
	struct aura_node *n = aura_open("dummy", NULL);
	struct aura_buffer *retbuf; 
	const struct aura_object *o; 

	aura_enable_sync_events(n, 5);

	while (count--) { 
		ret = aura_get_next_event(n, &o, &retbuf);
		slog(0, SLOG_DEBUG, "evt get ret %d", ret);
		
		aura_hexdump("Out buffer", retbuf->data, retbuf->size);
		aura_buffer_release(retbuf);
	}

	aura_close(n);

	return 0;
}


