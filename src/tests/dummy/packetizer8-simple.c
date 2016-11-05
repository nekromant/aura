#include <aura/aura.h>
#include <aura/packetizer.h>

static int numfails;
int expected[] =
{
	0x00,
	0x11,
	0x22,
	0x33,
	0x44,
	0x55,
};

static int curpacket;
void recvcb(struct aura_buffer *buf, void *arg)
{
	slog(0, SLOG_INFO, "Got a packet from packetizer");
	uint32_t result = aura_buffer_get_u32(buf);
	if (expected[curpacket]!=result) {
		slog(0, SLOG_ERROR, "expected %d got %d packet %d", expected[curpacket],
				result, curpacket);
		numfails++;
	}
	curpacket++;
	aura_buffer_release(buf);
}


void queue_packet(struct aura_packetizer *pkt, char *buf, int offset, uint32_t value)
{
	struct aura_packet8 *pck = (struct aura_packet8 *) &buf[offset];
	memcpy(pck->data, &value, sizeof(value));
	aura_packetizer_encapsulate(pkt, pck, 0x4);
	pkt->cont = 0;
	pkt->expect_cont = 0;
}

int main() {
	slog_init(NULL, 18);

	struct aura_node *node = aura_open("dummy", NULL);
	struct aura_packetizer *pkt = aura_packetizer_create(node);
	if (!pkt)
		BUG(node, "Fuck!");

	aura_packetizer_set_receive_cb(pkt, recvcb, NULL);
	char buf[1024];
	memset(buf,0x0, 1024 );


	queue_packet(pkt, buf, 10, expected[0]);
	/* packet overlapping the header */
	queue_packet(pkt, buf, 18+sizeof(struct aura_packet8), expected[1]);
	queue_packet(pkt, buf, 20+sizeof(struct aura_packet8), expected[1]);
	/* packet overlapping data */
	queue_packet(pkt, buf, 64, expected[2]);
	queue_packet(pkt, buf, 64 + sizeof(struct aura_packet8), expected[2]);

	queue_packet(pkt, buf, 90, expected[3]);
	int i;

	/* First, let's feed it byte by byte */
	for (i=0; i<256; i++)
		aura_packetizer_feed(pkt, &buf[i], 1);
	curpacket = 0;
	/* Next, in one chunk */
    aura_packetizer_feed(pkt, buf, 256);
	/* Clean up, let valgrind do it's job */
	aura_packetizer_destroy(pkt);
	aura_close(node);
	return numfails;
}
