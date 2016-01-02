#include <aura/aura.h>

int main() {
	slog_init(NULL, 18);

	int ret; 
	struct aura_node *n = aura_open("dummy", NULL);
	struct aura_buffer *retbuf; 

	/* We don't wait for status. Call should fail. Call shouldn't leak */
	//aura_wait_status(n, AURA_STATUS_ONLINE);

	ret = aura_call(n, "echo_u16", &retbuf, 0x0102);
	slog(0, SLOG_DEBUG, "call ret %d", ret);
	if (ret ==0)
		BUG(n, "This shouldn't happen");
	
	aura_close(n);	
	return 0;
}


