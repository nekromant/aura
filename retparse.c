#include <aura/aura.h>

#define DECLARE_GETFUNC(tp, name, swapfunc)				\
	tp aura_buffer_get_##name(struct aura_buffer *buf)		\
	{								\
		struct aura_node *node = buf->userdata;			\
		tp *result = (tp *) &buf->data[buf->pos];		\
									\
		buf->pos += sizeof(tp);					\
		if (buf->pos > buf->size)				\
			BUG(node, "attempt to access data beyound buffer boundary"); \
		if (node->need_endian_swap)				\
			*result = swapfunc(*result);			\
		return *result;						\
	}								\

#define noswap(v) v

DECLARE_GETFUNC(uint8_t, u8, noswap);
DECLARE_GETFUNC(int8_t,  s8, noswap);

DECLARE_GETFUNC(uint16_t, u16, __swap16);
DECLARE_GETFUNC(int16_t,  s16, __swap16);

DECLARE_GETFUNC(uint32_t, u32, __swap32);
DECLARE_GETFUNC(int32_t,  s32, __swap32);

DECLARE_GETFUNC(uint64_t, u64, __swap64);
DECLARE_GETFUNC(int64_t,  s64, __swap64);

void *aura_buffer_get_bin(struct aura_buffer *buf, int len)
{
	struct aura_node *node = buf->userdata;
	int pos = buf->pos;

	buf->pos += len;
	if (buf->pos > buf->size)
		BUG(node, "attempt to access data beyound buffer boundary");	
	return &buf->data[pos];
}
