#include <aura/aura.h>

int main() {
	slog_init(NULL, 18);

	int ret; 
	struct aura_node *n = aura_open("nmc", "./aura-test.abs");
	if (!n) {
		slog (0, SLOG_ERROR, "Failed to open node");
		exit(1);
	}

	aura_wait_status(n, AURA_STATUS_ONLINE);
	struct aura_buffer *retbuf; 

	ret = aura_call(n, "echo32", &retbuf, 0xdeadc0de);
	slog(0, SLOG_DEBUG, "call ret %d", ret);
	aura_hexdump("Out buffer", retbuf->data, retbuf->size);
	aura_buffer_release(n, retbuf);
	aura_close(n);

	return 0;
}
