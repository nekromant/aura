#include <aura/aura.h>

void test_u32(struct aura_node *n)
{
	int ret;
	struct aura_buffer *retbuf; 
	ret = aura_call(n, "echo32", &retbuf, 0xbeefc0de);
	if (ret != 0) 
		BUG(n, "Call failed!");

	uint32_t v = aura_buffer_get_u32(retbuf);
	if (v != 0xbeefc0de) {
		slog(0, SLOG_ERROR, "U32 test NOT ok: %llx vs %llx", v, 0xbeefc0de); 
	}
	aura_buffer_release(n, retbuf);
	slog(0, SLOG_INFO, "U32 echo test passed");
}

void test_bin(struct aura_node *n)
{
	char buf[64];
	int ret;
	FILE *fd = fopen("/dev/urandom", "r+");
	fread(buf, 64, 1, fd);
	fclose(fd);
	struct aura_buffer *retbuf; 

	ret = aura_call(n, "echobin", &retbuf, buf);
	if (ret != 0) 
		BUG(n, "Call failed!");

	if (0 != memcmp(buf, retbuf->data, 64))
		slog(0, SLOG_ERROR, "BIN test NOT ok"); 		
	aura_hexdump("Out buffer", buf, 64);
	aura_hexdump("In buffer", retbuf->data, 64);
	slog(0, SLOG_INFO, "BIN test passed");
}

void test_u64(struct aura_node *n)
{
	int ret;
	struct aura_buffer *retbuf; 
	ret = aura_call(n, "echo64", &retbuf, 0xbeefc0deb00bc0de);
	if (ret != 0) 
		BUG(n, "Call failed!");

	uint64_t v = aura_buffer_get_u64(retbuf);
	if (v != 0xbeefc0deb00bc0de)
		slog(0, SLOG_ERROR, "U64 test NOT ok: %llx vs %llx", v, 0xbeefc0deb00bc0de); 
	aura_buffer_release(n, retbuf);
	slog(0, SLOG_INFO, "U64 echo test passed");
}

void test_buf(struct aura_node *n)
{
	int ret;
	struct aura_buffer *retbuf; 
	struct aura_buffer *iobuf = aura_buffer_request(n, 80);
	uint32_t test = 0xdeadf00d;
	memcpy(iobuf->data, &test, sizeof(test));

	ret = aura_call(n, "echo_buf", &retbuf, iobuf);
	slog(0, SLOG_DEBUG, "call ret %d", ret);
	if (ret)
		BUG(n, "call failed");

	struct aura_buffer *tmp = aura_buffer_get_buf(retbuf);
	if (tmp != iobuf)
		BUG(n, "test not ok");

	aura_buffer_release(n, retbuf);
	aura_buffer_release(n, tmp);
	slog(0, SLOG_INFO, "BUF test passed");	
}

void test_u32u32(struct aura_node *n)
{
	int ret;
	struct aura_buffer *retbuf; 
	ret = aura_call(n, "echou32u32", &retbuf, 0xbeefc0de, 0xdeadc0de);
	if (ret != 0) 
		BUG(n, "Call failed!");

	uint32_t v1 = aura_buffer_get_u32(retbuf);
	uint32_t v2 = aura_buffer_get_u32(retbuf);

	if ((v1 != 0xbeefc0de) && (v2 != 0xdeadc0de)) {
		slog(0, SLOG_ERROR, "U32 test NOT ok: %llx,%llx vs %llx,%llx", 
		     v1, v2, 0xbeefc0de, 0xdeadc0de); 
	}

	aura_buffer_release(n, retbuf);
	slog(0, SLOG_INFO, "U32U32 echo test passed");
}


int main() {

	slog_init(NULL, 18);

	struct aura_node *n = aura_open("nmc", "./aura-test.abs");
	if (!n) {
		slog (0, SLOG_ERROR, "Failed to open node");
		exit(1);
	}
	aura_wait_status(n, AURA_STATUS_ONLINE);

	test_u32(n);
	test_u64(n);
	test_u32u32(n);
	test_bin(n);
	test_buf(n);

	aura_close(n);

	return 0;
}


