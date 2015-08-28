
/* TODO: Placeholder, We need a buffer pool to handle allocs here */ 
#include <aura/aura.h>
#include <string.h>
#include <stdlib.h>

struct aura_buffer *aura_buffer_internal_request(int size)
{
	return malloc(size);
}

void aura_buffer_internal_free(struct aura_buffer *buf)
{
	free(buf);
}
