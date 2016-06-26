#include <aura/aura.h>
#include <unistd.h>

int main() {
        int ret; 
	int i; 
	int passed = 0;
	int failed = 0;
        slog_init(NULL, 0);
        struct aura_node *n = aura_open("simpleusb", "../simpleusbconfigs/susb-test.conf");

        if (!n) { 
                printf("err\n");
                return -1;
        }

	i = 100; 
	while (i--) { 
		printf("Testing %d remaining\r", i);
		fflush(stdout);
		aura_wait_status(n, AURA_STATUS_ONLINE);
		struct aura_buffer *retbuf; 
		ret = aura_call(n, "led_ctl", &retbuf, 0x100, 0x100);
		if (0 == ret) {
			aura_buffer_release(retbuf); 
			passed++;
		} else
			failed++;
	}

        aura_close(n);

	printf("Stability test (no data): %d succeeded, %d failed total %d\n",
	       passed, failed, passed + failed);

        return failed;
}
