#include <aura/aura.h>
#include <unistd.h>

int main() {
	int ret; 
	int i = 3200; 
	slog_init(NULL, 88);
	struct aura_node *n = aura_open("usb", "1d50:6032;www.ncrmnt.org;;");


	aura_wait_status(n, AURA_STATUS_ONLINE);

	struct aura_buffer *retbuf; 
	ret = aura_call(n, "turnTheLedOn", &retbuf, 0x1);
	slog(0, SLOG_DEBUG, "call ret %d", ret);
	if (0 == ret) {
		printf("====> buf pos %d len %d\n", retbuf->pos, retbuf->size);
		ret = aura_buffer_get_u8(retbuf);
	}
	printf("====> GOT %d from device\n", ret);
	aura_buffer_release(n, retbuf); 

	/* Test if auto-created eventsystem will get properly destroyed */
	struct aura_eventloop *loop = aura_eventloop_create(n); 

	while(i--) {
		aura_handle_events(loop);
		usleep(10000);
	}

	aura_close(n);
	aura_eventloop_destroy(loop);
	return 0;
}
