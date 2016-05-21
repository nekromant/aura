#include <aura/aura.h>

#define ARG 100500
#define ARG2 100502

void calldonecb(struct aura_node *dev, int status, struct aura_buffer *retbuf, void *arg)
{
	printf("Call done with result %d arg %lld!\n", status, (long long unsigned int) arg);
	if (arg != (void *) ARG)
		exit(1);
	aura_hexdump("Out buffer", retbuf->data, retbuf->size);
	aura_eventloop_loopexit(aura_node_eventloop_get(dev), NULL);

}

static int numevt=0;
void pingcb(struct aura_node *dev, int status, struct aura_buffer *retbuf, void *arg)
{
	numevt++;
	printf("Event number %d with result %d arg %lld!\n", numevt, status, (long long unsigned int) arg);
	if (arg != (void *) ARG2)
		exit(1);
	aura_hexdump("Out buffer", retbuf->data, retbuf->size);
	if (numevt==4) {
		printf("Breaking the loop\n");
		aura_eventloop_loopexit(aura_node_eventloop_get(dev), NULL);
	}
}

int main() {
	slog_init(NULL, 18);

	int ret;
	struct aura_node *n = aura_open("dummy", NULL);
	aura_wait_status(n, AURA_STATUS_ONLINE);

	ret = aura_start_call(n, "echo_u16", calldonecb, (void *) ARG, 0x0102);
	printf("call started with ret %d\n", ret);
	if (ret !=0)
		return ret;

	ret = aura_set_event_callback(n, "ping", pingcb, (void *) ARG2);
	printf("event handler set with ret %d\n", ret);
	aura_eventloop_dispatch(aura_node_eventloop_get(n), 0);
	printf("Closing the shop...");
	aura_close(n);

	return 0;
}
