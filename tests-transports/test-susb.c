#include <aura/aura.h>
#include <unistd.h>
int main() {
        int ret; 
        int i = 32; 
        slog_init(NULL, 88);
        struct aura_node *n = aura_open("simpleusb", "simpleusbconfigs/susb-test.conf");
	struct aura_eventloop *loop = aura_eventloop_create(n); 

        if (!n) { 
                printf("err\n");
                return -1;
        }
        aura_wait_status(n, AURA_STATUS_ONLINE);

        struct aura_buffer *retbuf; 
	
        ret = aura_call(n, "id", &retbuf, 0, 500);
        slog(0, SLOG_DEBUG, "call ret %d", ret);
        if (0 == ret) {
                printf("====> buf pos %d len %d\n", retbuf->pos, retbuf->size);
                ret = aura_buffer_get_u32(retbuf);
        }
        printf("====> GOT %x from device\n", ret);
	aura_hexdump("buffer", retbuf->data, retbuf->size);
        aura_buffer_release(retbuf); 
	
	while(i--) {
		aura_handle_events(loop);
		usleep(10000);
        }
        aura_close(n);
        return 0;
}
