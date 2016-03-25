#include <aura/aura.h>
#include <aura/packetizer.h>


void recvcb(struct aura_buffer *buf, void *arg)
{
	slog(0, SLOG_INFO, "Got a packet from packetizer");
	aura_buffer_release(buf);
	exit(0);
}

int main() {
	slog_init(NULL, 18);

	struct aura_node *node = aura_open("dummy", NULL);
	struct aura_packetizer *pkt = aura_packetizer_create(node);
	if (!pkt)
		BUG(node, "Fuck!");

	aura_packetizer_set_receive_cb(pkt, recvcb, NULL);

	struct aura_packet8 *pck = malloc(1024);
	memset(pck, 0x0, 1024);
	aura_packetizer_encapsulate(pkt, pck, 0x10);
	int i;

	for (i=0; i<128; i++)
		aura_packetizer_feed(pkt, &((const char *) pck)[i], 1);

	return 0;
}
