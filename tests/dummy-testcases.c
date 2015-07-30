#include <aura/aura.h>


int main() {
	int ret; 
	slog_init(NULL, 8);
	struct aura_node *n = aura_open("dummy", 1, 2, 3);
	struct aura_buffer *retbuf; 


	ret = aura_call(n, "echo_u16", &retbuf, 0x0102);
	slog(0, SLOG_DEBUG, "call ret %d", ret);
	aura_buffer_release(n, retbuf);
	aura_close(n);	
	aura_hexdump("Out buffer", retbuf->data, retbuf->size);
	return 0;
}


