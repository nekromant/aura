#include <aura/aura.h>


int test_u16(struct aura_node *n)
{
	slog(0, SLOG_INFO, __FUNCTION__);
	int ret; 	
	struct aura_buffer *retbuf; 
	ret = aura_call(n, "echo_u16", &retbuf, 0xdead);
	if (ret)
		BUG(n, "Call failed");
	uint16_t out = aura_buffer_get_u16(retbuf);
	if (out != 0xdead)
		BUG(n, "Unexpected data from buffer");
	aura_buffer_release(n, retbuf);
}

int test_u32(struct aura_node *n)
{
	slog(0, SLOG_INFO, __FUNCTION__);
	int ret; 	
	struct aura_buffer *retbuf; 
	ret = aura_call(n, "echo_u32", &retbuf, 0xdeadb00b);
	if (ret)
		BUG(n, "Call failed");
	uint32_t out = aura_buffer_get_u32(retbuf);
	if (out != 0xdeadb00b)
		BUG(n, "Unexpected data from buffer");
	aura_buffer_release(n, retbuf);
}

int test_seq(struct aura_node *n)
{
	slog(0, SLOG_INFO, __FUNCTION__);
	int ret; 	
	struct aura_buffer *retbuf; 
	ret = aura_call(n, "echo_seq", &retbuf, 0xdeadb00b, 0xdead, 0xde);
	if (ret)
		BUG(n, "Call failed");
	uint32_t out32 = aura_buffer_get_u32(retbuf);
	uint16_t out16 = aura_buffer_get_u16(retbuf);
	uint8_t  out8 = aura_buffer_get_u8(retbuf);
	if ((out32 != 0xdeadb00b) || (out16 != 0xdead) || (out8 != 0xde)) {
		aura_hexdump("out buffer", retbuf->data, retbuf->size);
		slog(0, SLOG_ERROR, "===> 0x%x 0x%x 0x%x", out32, (uint32_t) out16, (uint32_t) out8);
		BUG(n, "Unexpected data from buffer");
	}
	aura_buffer_release(n, retbuf);
}


void test_bin_32_32(struct aura_node *n )
{
	unsigned char src0[32];
	unsigned char src1[32];
	unsigned char *dst;
	int ret;
	struct aura_buffer *retbuf; 
	int i;
	slog(0, SLOG_INFO, __FUNCTION__);

	memset(src0, 0xa, 32);
	memset(src1, 0xb, 32);

	ret = aura_call(n, "echo_bin", &retbuf, src0, src1);
	if (ret != 0) 
	    BUG(n, "Call failed!");

	
	dst=aura_buffer_get_bin(retbuf, 32);

	if (0 != memcmp(src0, dst, 32)) { 
		aura_hexdump("retbuf", retbuf->data, retbuf->size);
		BUG(n, "src0 mismatch");
	}

	dst=aura_buffer_get_bin(retbuf, 32);
	if (0 != memcmp(src1, dst, 32)) { 
		aura_hexdump("retbuf", retbuf->data, retbuf->size);
		BUG(n, "src1 mismatch");
	}

	aura_buffer_release(n, retbuf);
}


int main() {
	slog_init(NULL, 18);
	int ret; 
	struct aura_node *n = aura_open("dummy", NULL);
	test_u16(n);
	test_u32(n);
	test_seq(n);
	test_bin_32_32(n);
	aura_close(n);

	return 0;
}


