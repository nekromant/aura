#include <stdlib.h>
#include <unistd.h>

#ifdef AURA_HAS_LIBEVENT
#include <event.h>
#include <aura/aura.h>
void aura_buffer_put_eviovec(struct aura_buffer *buf, struct evbuffer_iovec *vec, size_t length)
{
	int i = 0;

	while (length) {
		size_t towrite = min_t(size_t, length, vec[i].iov_len);
		aura_buffer_put_bin(buf, vec[i].iov_base, towrite);
		i++;
		length -= towrite;
	}
}

struct aura_buffer *aura_buffer_from_eviovec(struct aura_node *		node,struct evbuffer_iovec *	vec, size_t			length)
{
	struct aura_buffer *buf = aura_buffer_request(node, length);

	if (!buf)
		return NULL;
	aura_buffer_put_eviovec(buf, vec, length);
	return buf;
}

#else
#include <aura/aura.h>
void aura_buffer_put_eviovec(struct aura_buffer *	buf,
			     struct evbuffer_iovec *	vec,
			     size_t			length)
{
	BUG(buf->owner, "libaura compiled without libevent support");
}

struct aura_buffer *aura_buffer_from_eviovec(struct aura_node *		node,
					     struct evbuffer_iovec *	vec,
					     size_t			length)
{
	BUG(node, "libaura compiled without libevent support");
}

#endif
