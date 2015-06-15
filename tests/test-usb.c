#include <aura/aura.h>


int main() {
	int ret; 
	init_slog(NULL, 8);
	struct aura_node *n = aura_open("usb", 0x1d50, 0x6032, NULL, NULL, NULL);
	struct aura_buffer *buf = aura_buffer_request(n, 23); 
	aura_wait_status(n, AURA_STATUS_ONLINE);

	struct aura_buffer *retbuf; 
	ret = aura_call(n, "turnTheLedOn", &retbuf, 0x1);
	slog(0, SLOG_DEBUG, "call ret %d", ret);
	while(1) 
		aura_loop_once(n);
	aura_close(n);
	return 0;
}


