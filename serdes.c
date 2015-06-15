#include <aura/aura.h>


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
 	return len + node->tr->buffer_overhead;
}

/* A relic of old days, replace with strncat impl */
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
	*str = 0x0;
	char *tmp = str;
	int shift = 0;
	int len; 
	int pos = 0; 
	while (*fmt) {
		pos+=shift; 
		tmp = &tmp[shift];
		shift = 0;
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
	tmp = &tmp[shift];
	return str;
}


//FixMe: This is likely to get messy due to integer promotion 
//       on different platforms

static inline void va_put_U8(struct aura_buffer *buf, va_list ap, bool swap)
{
	uint8_t v = (uint8_t) va_arg(ap, unsigned int);
	memcpy(&buf->data[buf->pos], &v, sizeof(v));
	buf->pos+=sizeof(v);
}

static inline void va_put_S8(struct aura_buffer *buf, va_list ap, bool swap)
{
	int8_t v = (int8_t) va_arg(ap, int);
	memcpy(&buf->data[buf->pos], &v, sizeof(v));
	buf->pos+=sizeof(v);
}

static inline void va_put_U16(struct aura_buffer *buf, va_list ap, bool swap)
{
	uint16_t v = (uint16_t) va_arg(ap, unsigned int);
	if (swap) 
		v = __swap16(v);
	memcpy(&buf->data[buf->pos], &v, sizeof(v));	
	buf->pos+=sizeof(v);
}

static inline void va_put_S16(struct aura_buffer *buf, va_list ap, bool swap)
{
	int16_t v = (int16_t) va_arg(ap, int);
	if (swap) 
		v = __swap16(v);
	memcpy(&buf->data[buf->pos], &v, sizeof(v));	
	buf->pos+=sizeof(v);
}

/* FixMe: Portability, we assume no promotion here for now */
static inline void va_put_U32(struct aura_buffer *buf, va_list ap, bool swap)
{
	uint32_t v = (uint32_t) va_arg(ap, uint32_t);
	if (swap) 
		v = __swap32(v);
	memcpy(&buf->data[buf->pos], &v, sizeof(v));	
	buf->pos+=sizeof(v);
}

static inline void va_put_S32(struct aura_buffer *buf, va_list ap, bool swap)
{
	int32_t v = (int32_t) va_arg(ap, uint32_t);
	if (swap) 
		v = __swap32(v);
	memcpy(&buf->data[buf->pos], &v, sizeof(v));	
	buf->pos+=sizeof(v);
}

static inline void va_put_U64(struct aura_buffer *buf, va_list ap, bool swap)
{
	uint64_t v = (uint64_t) va_arg(ap, uint64_t);
	if (swap) 
		v = __swap64(v);
	memcpy(&buf->data[buf->pos], &v, sizeof(v));	
	buf->pos+=sizeof(v);
}

static inline void va_put_S64(struct aura_buffer *buf, va_list ap, bool swap)
{
	int64_t v = (int64_t) va_arg(ap, uint64_t);
	if (swap) 
		v = __swap64(v);
	memcpy(&buf->data[buf->pos], &v, sizeof(v));
	buf->pos+=sizeof(v);	
}




struct aura_buffer *aura_serialize(struct aura_node *node, const char *fmt, va_list ap)
{
	int size = aura_fmt_len(node, fmt);
	struct aura_buffer *buf = aura_buffer_request(node, size);
	if (!buf)
		return NULL;

	aura_buffer_rewind(node, buf);

#define PUT(n)								\
	case URPC_ ## n:						\
		va_put_ ## n(buf, ap, node->need_endian_swap);		\
		break;							\
		
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
		};
	};

	return buf;
}
