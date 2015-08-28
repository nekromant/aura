#include <aura/aura.h>

#define ARG 100500
#define ARG2 100502

void calldonecb(struct aura_node *dev, int status, struct aura_buffer *retbuf, void *arg)
{
	printf("Call done with result %d arg %lld!\n", status, (long long unsigned int) arg);
	if (arg != (void *) ARG)
		exit(1);
	aura_hexdump("Out buffer", retbuf->data, retbuf->size);
}

static int numevt=0;
void unhandled_cb(struct aura_node *dev, 
		  struct aura_buffer *retbuf, 
		  void *arg)
{
	const struct aura_object *o = aura_get_current_object(dev);
	numevt++;
	printf("Unhandled event name %s %d with arg %lld!\n",
	       o->name, numevt, (long long unsigned int) arg);
	if (arg != (void *) ARG2)
		exit(1);

	aura_hexdump("Out buffer", retbuf->data, retbuf->size);
	if (numevt==4) {
		printf("Breaking the loop\n");
		aura_eventloop_break(aura_eventloop_get_data(dev));
	}
}

int main() {
	slog_init(NULL, 18);

	int ret; 
	struct aura_node *n = aura_open("dummy", 1, 2, 3);
	ret = aura_start_call(n, "echo_u16", calldonecb, (void *) ARG, 0x0102);
	printf("call started with ret %d\n", ret);

	aura_unhandled_evt_cb(n, unhandled_cb, (void *) ARG2);
	printf("event handler set with ret %d\n", ret);
	aura_handle_events_forever(aura_eventloop_get_data(n));
	printf("Closing teh shop...");
	aura_close(n);

	return 0;
}


