#include <aura/aura.h>

int main() {
	int ret; 
	init_slog(NULL, 88);
	struct aura_node *n = aura_open("usb", 0x1d50, 0x6032, "www.ncrmnt.org", NULL, NULL);
	struct aura_buffer *buf = aura_buffer_request(n, 23); 
	aura_wait_status(n, AURA_STATUS_ONLINE);

	struct aura_buffer *retbuf; 
	ret = aura_call(n, "turnTheLedOn", &retbuf, 0x1);
	slog(0, SLOG_DEBUG, "call ret %d", ret);
	if (0 == ret) {
		printf("====> buf pos %d len %d\n", retbuf->pos, retbuf->size);
		ret = aura_buffer_get_u8(retbuf);
	}
	printf("====> GOT %d from device\n", ret);
	
//	aura_hexdump("Out buffer", retbuf->data, retbuf->size);
	while(1) {
		aura_loop_once(n);
		sleep(1);
	}
	aura_close(n);
	return 0;
}


