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
		struct aura_buffer *retbuf;
		uint32_t a = rand();
		uint32_t b = rand();
		aura_wait_status(n, AURA_STATUS_ONLINE);

		ret = aura_call(n, "write", &retbuf, 0xa, 0xb, a, b);
		if (0 != ret) {
			failed++;
			continue;
		}

		aura_buffer_release(retbuf); 
		ret = aura_call(n, "read", &retbuf, 0x0, 0x0);
		if (0 != ret) {
			failed++;
			continue;
		}

		uint32_t tmp = aura_buffer_get_u32(retbuf);
		if (a != tmp) {
			slog(0, SLOG_ERROR, "%x != %x", tmp, a);
			failed++;
			aura_buffer_release(retbuf); 
			continue;
		}

		tmp = aura_buffer_get_u32(retbuf);
		if (b != tmp) {
			slog(0, SLOG_ERROR, "%x != %x", tmp, b);
			failed++;
			aura_buffer_release(retbuf); 
			continue;
		}

		aura_buffer_release(retbuf); 
		passed++;
	}

        aura_close(n);

	printf("Stability test (data io): %d succeeded, %d failed total %d\n",
	       passed, failed, passed + failed);

        return failed;
}
