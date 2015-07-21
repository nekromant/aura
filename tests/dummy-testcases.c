#include <aura/aura.h>


int main() {
	int ret; 
	slog_init(NULL, 8);
	struct aura_node *n = aura_open("dummy", 1, 2, 3);
	struct aura_buffer *buf = aura_buffer_request(n, 23); 
	struct aura_eventloop *loop = aura_eventloop_create(n); 

	ret = aura_queue_call(n, 0, NULL, NULL, buf);
	aura_handle_events(loop);
	slog(0, SLOG_DEBUG, "<---------------->");

	struct aura_buffer *retbuf; 
	ret = aura_call(n, "echo_u16", &retbuf, 0x0102);
	slog(0, SLOG_DEBUG, "call ret %d", ret);
	aura_panic(n);
	aura_close(n);	
	aura_hexdump("Out buffer", retbuf->data, retbuf->size);
	return 0;
}


