#include <aura/aura.h>
#include <aura/private.h>

#define DECLARE_GETFUNC(tp, name, swapfunc)                             \
	tp aura_buffer_get_ ## name(struct aura_buffer *buf)              \
	{                                                               \
		struct aura_node *node = buf->owner;                    \
		tp result = *(tp *)&buf->data[buf->pos];               \
                                                                        \
		buf->pos += sizeof(tp);                                 \
		if (buf->pos > buf->size)                               \
			BUG(node, "attempt to access data beyound buffer boundary"); \
		if (node->need_endian_swap)                             \
			result = swapfunc(result);                      \
		return result;                                          \
	}                                                               \

#define noswap(v) v

#define DECLARE_PUTFUNC(tp, name, swapfunc)                             \
	void aura_buffer_put_ ## name(struct aura_buffer *buf, tp value)  \
	{                                                               \
		struct aura_node *node = buf->owner;                    \
		tp *target = (tp *)&buf->data[buf->pos];               \
                                                                        \
		if (node->need_endian_swap)                             \
			value = swapfunc(value);                        \
                                                                        \
		if (buf->pos > buf->size)                               \
			BUG(node, "attempt to access data beyound buffer boundary"); \
		buf->pos += sizeof(tp);                                 \
                                                                        \
		*target = value;                                        \
	}                                                               \

DECLARE_GETFUNC(uint8_t, u8, noswap);
DECLARE_GETFUNC(int8_t, s8, noswap);

DECLARE_GETFUNC(uint16_t, u16, __swap16);
DECLARE_GETFUNC(int16_t, s16, __swap16);

DECLARE_GETFUNC(uint32_t, u32, __swap32);
DECLARE_GETFUNC(int32_t, s32, __swap32);

DECLARE_GETFUNC(uint64_t, u64, __swap64);
DECLARE_GETFUNC(int64_t, s64, __swap64);

DECLARE_PUTFUNC(uint8_t, u8, noswap);
DECLARE_PUTFUNC(int8_t, s8, noswap);

DECLARE_PUTFUNC(uint16_t, u16, __swap16);
DECLARE_PUTFUNC(int16_t, s16, __swap16);

DECLARE_PUTFUNC(uint32_t, u32, __swap32);
DECLARE_PUTFUNC(int32_t, s32, __swap32);

DECLARE_PUTFUNC(uint64_t, u64, __swap64);
DECLARE_PUTFUNC(int64_t, s64, __swap64);

const void *aura_buffer_get_bin(struct aura_buffer *buf, int len)
{
	struct aura_node *node = buf->owner;
	int pos = buf->pos;

	buf->pos += len;
	if (buf->pos > buf->size)
		BUG(node, "attempt to access data beyound buffer boundary");
	return &buf->data[pos];
}

void aura_buffer_put_bin(struct aura_buffer *buf, const void *data, int len)
{
	struct aura_node *node = buf->owner;
	int pos = buf->pos;

	if (buf->pos > buf->size)
		BUG(node, "attempt to access data beyound buffer boundary");

	memcpy(&buf->data[pos], data, len);
	buf->pos += len;
}

struct aura_buffer *aura_buffer_get_buf(struct aura_buffer *buf)
{
	struct aura_node *node = buf->owner;
	struct aura_buffer *ret;

	if (!node->tr->buffer_get)
		BUG(node, "This node doesn't support aura_buffer as argument");
	ret = node->tr->buffer_get(buf);
	if (ret->magic != AURA_BUFFER_MAGIC_ID)
		BUG(node, "Fetched an aura_buffer with invalid magic - check your code!");
	return ret;
}
