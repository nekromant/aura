#include <aura/aura.h>

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

#define TRANSPORT "dummy"

long current_time(void)
{
	long            ms; // Milliseconds
    time_t          s;  // Seconds
    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);

    s  = spec.tv_sec;
    ms = s * 1000 + round(spec.tv_nsec / 1.0e6); // Convert nanoseconds to milliseconds

    //return (long) clock();
	return ms;
}
//Shut up unhandled warning
void unhandled_cb(struct aura_node *dev,
		  struct aura_buffer *retbuf,
		  void *arg)
{

}


long run_first(struct aura_node *n)
{
	struct aura_buffer *retbuf;
	int i;
	long start = current_time();
	for (i=0; i<90000; i++) {
		aura_call(n, "echo_u16", &retbuf, 0x0102);
		aura_buffer_release(retbuf);
		aura_call(n, "echo_u8", &retbuf, 0x0102);
		aura_buffer_release(retbuf);
		aura_call(n, "echo_u32", &retbuf, 0x0102);
		aura_buffer_release(retbuf);
		aura_call(n, "echo_u64", &retbuf, 0x0102);
		aura_buffer_release(retbuf);
	}
	return current_time() - start;

}

long run_second(struct aura_node *n)
{
	struct aura_buffer *buf;
	long start = current_time();
	int i;
	for (i=0; i<90000; i++) {
		buf = aura_buffer_request(n, (rand() % 512) + 1);
		aura_buffer_release(buf);
	}
	return current_time() - start;
}

void average_aggregate(struct aura_node *n, long (*test)(struct aura_node *n), int runs, char *lbl)
{
	long v = 0;
	int i;
	for (i=0; i<runs; i++)
		v+=test(n);
	v/=runs;
	printf("%lu \t ms avg of %d runs (%s)\n", v, runs, lbl);
}

int main() {
	slog_init(NULL, 0);
	int num_runs = 5;
#ifdef AURA_USE_BUFFER_POOL
	printf("Buffer pool enabled!");
#else
	printf("Buffer pool disabled!");
#endif

	struct aura_node *n = aura_open(TRANSPORT, NULL);
	printf("+GC -PREHEAT\n");
	aura_unhandled_evt_cb(n, unhandled_cb, (void *) 0);
	average_aggregate(n, run_first, num_runs, "call test");
//	average_aggregate(n, run_second, num_runs, "alloc/dealloc test");
	aura_close(n);

	n = aura_open(TRANSPORT, NULL);

	printf("+GC +PREHEAT\n");
	aura_unhandled_evt_cb(n, unhandled_cb, (void *) 0);
	aura_bufferpool_preheat(n, 512, 10);
	average_aggregate(n, run_first, num_runs, "call test");
//	average_aggregate(n, run_second, num_runs, "alloc/dealloc test");
	aura_close(n);

	n = aura_open(TRANSPORT, NULL);

	printf("-GC -PREHEAT\n");
	aura_unhandled_evt_cb(n, unhandled_cb, (void *) 0);
	n->gc_threshold = -1;
	average_aggregate(n, run_first, num_runs, "call test");
//	average_aggregate(n, run_second, num_runs, "alloc/dealloc test");
	aura_close(n);

	return 0;
}


