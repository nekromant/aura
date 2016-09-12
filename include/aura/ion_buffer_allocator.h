#ifndef _ION_BUFFER_ALLOCATOR_H
#define _ION_BUFFER_ALLOCATOR_H

#include <ion/ion.h>

struct aura_ion_buffer_descriptor {
	int			map_fd;
	int			share_fd;
	int			size;
	ion_user_handle_t	hndl;
	struct aura_buffer	buf;
};

struct aura_ion_allocator_data {
	int ion_fd;
};

#include <aura/buffer_allocator.h>
extern struct aura_buffer_allocator g_aura_ion_buffer_allocator;

#define AURA_NODE_ION_BUFFER_ALLOCATOR &g_aura_ion_buffer_allocator

#endif
