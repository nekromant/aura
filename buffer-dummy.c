
/* TODO: Placeholder, We need a buffer pool to handle allocs here */ 
#include <aura/aura.h>
#include <string.h>
#include <stdlib.h>

struct aura_buffer *aura_buffer_internal_request(int size)
{
	int act_size = sizeof(struct aura_buffer) + size; 
	struct aura_buffer *ret = malloc(act_size);

	ret->size = size;
	ret->pos = 0;
	return ret; 
}

void aura_buffer_internal_free(struct aura_buffer *buf)
{
	free(buf);
}
