#include <aura/aura.h>
#include <unistd.h>


void ctl(struct aura_node *node, int out, int reg, int state)
{
        struct aura_buffer *retbuf = NULL; 
        aura_call(node, "bit_set", &retbuf, out << 8 | reg, state);
	if (retbuf)
		aura_buffer_release(retbuf); 	
}

int main(int argc, const char **argv) {

        slog_init(NULL, 88);

        struct aura_node *n = aura_open("simpleusb", "simpleusbconfigs/pw-ctl.conf");
        if (!n) { 
                printf("err\n");
                return -1;
        }

        aura_wait_status(n, AURA_STATUS_ONLINE);

	ctl(n, atoi(argv[1]), atoi(argv[2]), atoi(argv[3]));

        aura_close(n);
        return 0;
}
