#include <aura/aura.h>

int main() {
        int ret; 
        int i = 1632; 
        init_slog(NULL, 88);
        struct aura_node *n = aura_open("simpleusb", "simpleusbconfigs/pw-ctl.conf");
        if (!n) { 
                printf("err\n");
                return -1;
        }
        aura_wait_status(n, AURA_STATUS_ONLINE);

        struct aura_buffer *retbuf; 
        ret = aura_call(n, "load", &retbuf, 0x1, 0x2);
        slog(0, SLOG_DEBUG, "call ret %d", ret);
        if (0 == ret) {
                printf("====> buf pos %d len %d\n", retbuf->pos, retbuf->size);
                ret = aura_buffer_get_u8(retbuf);
        }
        printf("====> GOT %d from device\n", ret);
        aura_buffer_release(n, retbuf); 
        while(i--) {
                aura_loop_once(n);
                usleep(10000);
        }
        aura_close(n);
        return 0;
}
