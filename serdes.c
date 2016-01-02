#include <aura/aura.h>
#include <aura/private.h>


/**
 * Returns the length of the buffer required to serialize the data of the following format
 * This doesn't include any transport-specific overhead
 *
 * @param node
 * @param fmt
 * @return
 */
int  aura_fmt_len(struct aura_node *node, const char *fmt)
{
	int len=0;
	int tmp; 

	if (!fmt)
		return 0;

	while (*fmt) {
		switch (*fmt++) { 
		case URPC_U8:
 		case URPC_S8:
			len += 1;
			break;
		case URPC_U16:
 		case URPC_S16:
			len += 2;
			break;
		case URPC_U32:
 		case URPC_S32:
			len += 4;
			break;
		case URPC_U64:
 		case URPC_S64:
		case URPC_BUF:
			len += 8;
			break;
		case URPC_BIN: 
			tmp = atoi(fmt);
			if (tmp == 0) 
				BUG(node, "Internal serilizer bug processing: %s", fmt);
			len += tmp; 
			while (*fmt && (*fmt++ != '.'));
			break;
		default:
			BUG(node, "Serializer failed at token: %s", fmt); 
		}
	}
 	return len;
}

/**
 * Return a pretty-printed representation of format in an allocated string.
 * This function also validates the format and calculates the number of args
 * The user should take care to free the string
 *
 * FixME: This implementation is old and hacky and should be refactored
 *
 * @param fmt string representing argument format
 * @param valid pointer to an integer. It will contain 1 if the supplied format
 *        is valid, 0 if not.
 * @param num_args The number of arguments this format contains
 * @return
 */
char* aura_fmt_pretty_print(const char* fmt, int *valid, int *num_args)
{
	*valid = 1;
	*num_args = 0;
	if (!fmt) {
		char* str; 
		asprintf(&str, "(null)");
		return str;
	}
	char *str = malloc(strlen(fmt) * 10 + 64);
	if (!str)
		aura_panic(NULL);
	*str = 0x0;
	char *tmp = str;
	int shift = 0;
	int len; 
	int pos = 0; 
	while (*fmt) {
		pos+=shift; 
		tmp = &tmp[shift];
		switch (*fmt++) { 
		case URPC_U8: 
			shift = sprintf(tmp, " uint8_t");
			break;
		case URPC_U16:
			shift = sprintf(tmp, " uint16_t");
			break;
		case URPC_U32:
			shift = sprintf(tmp, " uint32_t");
			break;
		case URPC_U64:
			shift = sprintf(tmp, " uint64_t");
			break;
			
		case URPC_S8:
			shift = sprintf(tmp, " int8_t");
			break;
		case URPC_S16:
			shift = sprintf(tmp, " int16_t");
			break;
		case URPC_S32:
			shift = sprintf(tmp, " int32_t");
			break;
		case URPC_S64:
			shift = sprintf(tmp, " int64_t");
			break;
		case URPC_BUF:
			shift = sprintf(tmp, " buf");
			break;
		case URPC_BIN: 
			len = atoi(fmt);
			if (len == 0) 
				BUG(NULL, "Internal serilizer bug processing: %s", fmt);
			shift = sprintf(tmp, " bin(%d)", len);
			while (*fmt && (*fmt++ != '.'));
			break;
		case 0x0:
			shift = sprintf(tmp, " (null)");
			break;
		default:
			*valid = 0;
			shift = sprintf(tmp, " ?[%c]?", *fmt);
			fmt++;
		}
		++(*num_args);
	}
	return str;
}



//FixMe: This is likely to get messy due to integer promotion 
//       on different platforms

#define va_put_U8(buf, ap, swap)				\
	{							\
		uint8_t v = (uint8_t) va_arg(ap, unsigned int); \
		memcpy(&buf->data[buf->pos], &v, sizeof(v));	\
		buf->pos+=sizeof(v);				\
	}

#define va_put_S8(buf, ap, swap)				\
	{							\
		int8_t v = (int8_t) va_arg(ap, int);		\
		memcpy(&buf->data[buf->pos], &v, sizeof(v));	\
		buf->pos+=sizeof(v);				\
	}							\

#define va_put_U16(buf, ap, swap)					\
	{								\
		uint16_t v = (uint16_t) va_arg(ap, unsigned int);	\
		if (swap)						\
			v = __swap16(v);				\
		memcpy(&buf->data[buf->pos], &v, sizeof(v));		\
		buf->pos+=sizeof(v);					\
	}

#define va_put_S16(buf, ap, swap)				\
	{							\
		int16_t v = (int16_t) va_arg(ap, int);		\
		if (swap)					\
			v = __swap16(v);			\
		memcpy(&buf->data[buf->pos], &v, sizeof(v));	\
		buf->pos+=sizeof(v);				\
	}

/* FixMe: Portability, we assume no promotion here for now */
#define va_put_U32(buf, ap, swap)				\
	{							\
		uint32_t v = (uint32_t) va_arg(ap, uint32_t);	\
		if (swap)					\
			v = __swap32(v);			\
		memcpy(&buf->data[buf->pos], &v, sizeof(v));	\
		buf->pos+=sizeof(v);				\
	}

#define va_put_S32(buf, ap, swap)				\
	{							\
		int32_t v = (int32_t) va_arg(ap, int32_t);	\
		if (swap)					\
			v = __swap32(v);			\
		memcpy(&buf->data[buf->pos], &v, sizeof(v));	\
		buf->pos+=sizeof(v);				\
	}

#define va_put_U64(buf, ap, swap)				\
	{							\
		uint64_t v = (uint64_t) va_arg(ap, uint64_t);	\
		if (swap)					\
			v = __swap64(v);			\
		memcpy(&buf->data[buf->pos], &v, sizeof(v));	\
		buf->pos+=sizeof(v);				\
	}

#define va_put_S64(buf, ap, swap)				\
	{							\
		int64_t v = (int64_t) va_arg(ap, uint64_t);	\
		if (swap)					\
			v = __swap64(v);			\
		memcpy(&buf->data[buf->pos], &v, sizeof(v));	\
		buf->pos+=sizeof(v);				\
	}							\

#define va_put_BIN(buf, len, ap)			\
	{						\
		void *ptr = va_arg(ap, void *);		\
		memcpy(&buf->data[buf->pos], ptr, len);	\
		buf->pos+=len;				\
	}

#define va_put_BUF(buf, ap, swap)					\
	{								\
		struct aura_buffer *out = va_arg(ap, void *);		\
		struct aura_node *_node = buf->owner;			\
		if (!_node->tr->buffer_put)				\
			BUG(_node, "This node doesn't support aura_buffer as argument"); \
		_node->tr->buffer_put(buf, out);			\
	}


/**
 * Serialize a va_list ap of arguments according to format in an allocated aura_buffer
 * This function takes care to do all the needed endian swapping and buffer overhead handling.
 *
 * @param node
 * @param fmt
 * @param ap
 * @return
 */
struct aura_buffer *aura_serialize(struct aura_node *node, const char *fmt, int size, va_list ap)
{
	struct aura_buffer *buf = aura_buffer_request(node, size);
	if (!buf)
		return NULL;

#define PUT(n)							\
	case URPC_ ## n:					\
		va_put_ ## n(buf, ap, node->need_endian_swap);	\
		break;						\
		
	while (*fmt) {
		switch (*fmt++) { 
			PUT(U8);
			PUT(S8);
			PUT(U16);
			PUT(S16);
			PUT(U32);
			PUT(S32);
			PUT(U64);
			PUT(S64);
			PUT(BUF);
		case URPC_BIN:
		{
			int len = atoi(fmt);
			if (len == 0) 
				BUG(NULL, "Internal serilizer bug processing: %s", fmt);
			va_put_BIN(buf, len, ap);
			while (*fmt && (*fmt++ != '.'));
			break;
		}
		};
	};
	return buf;
}
