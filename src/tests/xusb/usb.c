#include <aura/aura.h>
#include <unistd.h>

int main() {
	int ret; 
	slog_init(NULL, 88);
	struct aura_node *n = aura_open("usb", "1d50:6032;www.ncrmnt.org;aura-usb-test-rig;");

	aura_wait_status(n, AURA_STATUS_ONLINE);
	aura_enable_sync_events(n, 5);

	struct aura_buffer *retbuf; 
	ret = aura_call(n, "turnTheLedOn", &retbuf, 0x1);
	slog(0, SLOG_DEBUG, "call ret %d", ret);
	if (0 != ret)
		exit(1);

	printf("====> buf pos %d len %d\n", retbuf->pos, retbuf->size);
	ret = aura_buffer_get_u8(retbuf);
	printf("====> GOT %d from device\n", ret);

	aura_buffer_release(retbuf); 

	const struct aura_object *o; 
	ret = aura_get_next_event(n, &o, &retbuf);
	if (ret != 0)
		exit(1);
	printf("===> got event: %s, ok\n", o->name);

	aura_buffer_release(retbuf); 	
	aura_close(n);
	return 0;
}
