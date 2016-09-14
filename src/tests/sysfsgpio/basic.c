#include <aura/aura.h>

int main() {
	slog_init(NULL, 18);

	int ret; 
	struct aura_node *n = aura_open("dummy", NULL);
	struct aura_buffer *retbuf; 
	aura_wait_status(n, AURA_STATUS_ONLINE);

	ret = aura_call(n, "echo_u16", &retbuf, 0x0102);
	slog(0, SLOG_DEBUG, "call ret %d", ret);
	if (ret !=0)
		return ret;
	
	aura_hexdump("Out buffer", retbuf->data, retbuf->size);
	aura_buffer_release(retbuf);
	aura_close(n);
	
	return 0;
}


