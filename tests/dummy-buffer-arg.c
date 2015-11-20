#include <aura/aura.h>

int main() {
	slog_init(NULL, 18);

	int ret; 
	struct aura_node *n = aura_open("dummy", NULL);
	struct aura_buffer *retbuf; 
	struct aura_buffer *iobuf = aura_buffer_request(n, 80);

	ret = aura_call(n, "echo_buf", &retbuf, iobuf);
	slog(0, SLOG_DEBUG, "call ret %d", ret);
	if (ret)
		BUG(n, "call failed");
	struct aura_buffer *tmp = aura_buffer_get_buf(retbuf);
	if (tmp != iobuf)
		BUG(n, "test not ok");

	aura_buffer_release(n, retbuf);
	aura_buffer_release(n, tmp);
	aura_close(n);

	return 0;
}


