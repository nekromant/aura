#include <aura/aura.h>

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

long current_time(void)
{
    long            ms; // Milliseconds
    time_t          s;  // Seconds
    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);

    s  = spec.tv_sec;
    ms = s * 1000 + round(spec.tv_nsec / 1.0e6); // Convert nanoseconds to milliseconds
    return ms;
}
//Shut up unhandled warning
void unhandled_cb(struct aura_node *dev,
		  struct aura_buffer *retbuf,
		  void *arg)
{

}


inline void run_first(struct aura_node *n)
{
	struct aura_buffer *retbuf;
	int ret;
	int i;
	long start = current_time();
	for (i=0; i<100000; i++) {
		ret = aura_call(n, "echo_u16", &retbuf, 0x0102);
		aura_buffer_release(retbuf);
		ret = aura_call(n, "echo_u8", &retbuf, 0x0102);
		aura_buffer_release(retbuf);
		ret = aura_call(n, "echo_u32", &retbuf, 0x0102);
		aura_buffer_release(retbuf);
		ret = aura_call(n, "echo_u64", &retbuf, 0x0102);
		aura_buffer_release(retbuf);
	}
	long elapsed = current_time() - start;
	printf("First test completed in %lu ms\n", elapsed);
}

inline void run_second(struct aura_node *n)
{
	struct aura_buffer *buf;
	long start = current_time();
	int i;
	for (i=0; i<900000; i++) {
		buf = aura_buffer_request(n, (rand() % 512) + 1);
		aura_buffer_release(buf);
	}
	long elapsed = current_time() - start;
	printf("Second test completed in %lu ms\n", elapsed);
}

int main() {
	slog_init(NULL, 0);

	int i;
	struct aura_node *n = aura_open("dummy", NULL);
	aura_unhandled_evt_cb(n, unhandled_cb, (void *) 0);

	run_first(n);
	run_second(n);
	aura_close(n);

	return 0;
}


