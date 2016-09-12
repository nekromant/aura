#include <aura/aura.h>
#include <aura/ion_buffer_allocator.h>
#include <sys/mman.h>
#include <unistd.h>


static void *ion_buffer_alloc_create(struct aura_node *node)
{
	struct aura_ion_allocator_data *data = calloc(sizeof(*data), 1);

	if (!data)
		BUG(node, "Memory allocation failure");

	data->ion_fd = ion_open();

	if (data->ion_fd < 0) {
		slog(0, SLOG_ERROR, "Failed to init ion: %s", strerror(errno));
		goto errfreemem;
	}

	return data;

errfreemem:
	free(data);
	return NULL;
}

static void ion_buffer_alloc_destroy(struct aura_node *node, void *data)
{
	struct aura_ion_allocator_data *pv = data;

	close(pv->ion_fd);
	free(pv);
}

static struct aura_buffer *ion_buffer_request(struct aura_node *node, void *data, int size)
{
	int ret;
	int map_fd = 0;
	ion_user_handle_t hndl = 0;
	struct aura_ion_allocator_data *pv = data;

	struct aura_ion_buffer_descriptor *dsc = malloc(sizeof(*dsc));

	if (!dsc)
		BUG(node, "malloc failed!");

	if (size) {
		ret = ion_alloc(pv->ion_fd, size, 0x8, 0xf, 0, &hndl);
		if (ret)
			BUG(node, "ION allocation of %d bytes failed: %d", size, ret);

		ret = ion_map(pv->ion_fd, hndl, size, (PROT_READ | PROT_WRITE),
			      MAP_SHARED, 0, (void *)&dsc->buf.data, &map_fd);
		if (ret)
			BUG(node, "ION mmap failed");
	} else {
		dsc->buf.data = NULL;
	}

	dsc->map_fd = map_fd;
	dsc->hndl = hndl;
	dsc->size = size;
	dsc->share_fd = -1;
	return &dsc->buf;
}

static void ion_buffer_release(struct aura_node *node, void *data, struct aura_buffer *buf)
{
	struct aura_ion_allocator_data *pv = data;
	struct aura_ion_buffer_descriptor *dsc = container_of(buf, struct aura_ion_buffer_descriptor, buf);
	int ret;

	if (dsc->buf.data) {
		munmap(dsc->buf.data, dsc->size);

		ret = ion_free(pv->ion_fd, dsc->hndl);
		if (ret)
			BUG(node, "Shit happened when doing ion_free(): %d", ret);

		close(dsc->map_fd);
	}
	free(dsc);
}


struct aura_buffer_allocator g_aura_ion_buffer_allocator = {
	.name		= "ion",
	.create		= ion_buffer_alloc_create,
	.request	= ion_buffer_request,
	.release	= ion_buffer_release,
	.destroy	= ion_buffer_alloc_destroy
};
