#include <aura/aura.h>
#include <unistd.h>

int main() {
        int ret; 
        int i = 2; 
        slog_init(NULL, 88);

        struct aura_node *n = aura_open("simpleusb", "./simpleusbconfigs/pw-ctl.conf");

        if (!n) { 
                printf("err\n");
                return -1;
        }
        aura_wait_status(n, AURA_STATUS_ONLINE);

        struct aura_buffer *retbuf; 
        ret = aura_call(n, "bit_set", &retbuf, 12<<8 | 1, 1);
        slog(0, SLOG_DEBUG, "call ret %d", ret);
        if (0 == ret) {
                printf("====> buf pos %d len %d\n", retbuf->pos, retbuf->size);
		aura_buffer_release(n, retbuf); 
        }

	int v = 1; 	
	while(1) {
		v = !v;
		printf("<=================>\n");
		ret = aura_call(n, "bit_set", &retbuf, 12<<8, v);
		slog(0, SLOG_DEBUG, "call ret %d", ret);
		if (0 == ret) {
			printf("====> buf pos %d len %d\n", retbuf->pos, retbuf->size);
			aura_buffer_release(n, retbuf); 
		} else {
			aura_wait_status(n, AURA_STATUS_ONLINE);
			i--;
			if (!i)
				goto bailout;
		}
		sleep(1);
        }
bailout:
        aura_close(n);
        return 0;
}
