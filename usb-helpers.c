#include <aura/aura.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <libusb.h>
#include <aura/usb_helpers.h>


void ncusb_print_libusb_transfer(struct libusb_transfer *p_t)
{
	if (NULL == p_t) {
		slog(4, SLOG_INFO, "No libusb_transfer...");
	} else {
		slog(4, SLOG_INFO, "libusb_transfer structure:");
		slog(4, SLOG_INFO, "flags =%x ", p_t->flags);
		slog(4, SLOG_INFO, "endpoint=%x ", p_t->endpoint);
		slog(4, SLOG_INFO, "type =%x ", p_t->type);
		slog(4, SLOG_INFO, "timeout =%d ", p_t->timeout);
// length, and buffer are commands sent to the device
		slog(4, SLOG_INFO, "length =%d ", p_t->length);
		slog(4, SLOG_INFO, "actual_length =%d ", p_t->actual_length);
		slog(4, SLOG_INFO, "buffer =%p ", p_t->buffer);
		slog(4, SLOG_INFO, "status =%d/%d ", p_t->status, LIBUSB_TRANSFER_OVERFLOW);
	}
	return;
}


int ncusb_match_string(libusb_device_handle *dev, int index, const char *string)
{
	unsigned char tmp[256];
	int ret = libusb_get_string_descriptor_ascii(dev, index, tmp, 256);

	if (ret <= 0)
		return 0;

	slog(4, SLOG_DEBUG, "cmp idx %d str %s vs %s", index, tmp, string);
	if (string == NULL)
		return 1; /* NULL matches anything */
	return strcmp(string, (char *)tmp) == 0;
}


struct libusb_device_handle *ncusb_try_device(struct libusb_context *ctx,
					      libusb_device *device,
					      int vid, int pid,
					      const char *vendor_name,
					      const char *product_name,
					      const char *serial)
{
	int err;
	libusb_device_handle *handle;
	struct libusb_device_descriptor desc;

	err = libusb_open(device, &handle);
	if (err)
		return NULL;

	err = libusb_get_device_descriptor(device, &desc);
	if (err)
		goto errclose;

	if (desc.idVendor == vid && desc.idProduct == pid &&
	    ncusb_match_string(handle, desc.iManufacturer, vendor_name) &&
	    ncusb_match_string(handle, desc.iProduct, product_name) &&
	    ncusb_match_string(handle, desc.iSerialNumber, serial)
	    )
		return handle;

errclose:
	libusb_close(handle);
	return NULL;
}

struct libusb_device_handle *ncusb_find_and_open(struct libusb_context *ctx,
						 int vendor, int product,
						 const char *vendor_name,
						 const char *product_name,
						 const char *serial)
{
	libusb_device_handle *found = NULL;
	libusb_device **list;
	ssize_t cnt = libusb_get_device_list(ctx, &list);
	ssize_t i = 0;

	if (cnt < 0) {
		slog(0, SLOG_ERROR, "no usb devices found");
		return NULL;
	}

	for (i = 0; i < cnt; i++) {
		int err = 0;
		libusb_device *device = list[i];
		struct libusb_device_descriptor desc;
		libusb_device_handle *handle;
		err = libusb_open(device, &handle);
		if (err)
			continue;

		int r = libusb_get_device_descriptor(device, &desc);
		if (r) {
			libusb_close(handle);
			continue;
		}

		if (desc.idVendor == vendor && desc.idProduct == product &&
		    ncusb_match_string(handle, desc.iManufacturer, vendor_name) &&
		    ncusb_match_string(handle, desc.iProduct, product_name) &&
		    ncusb_match_string(handle, desc.iSerialNumber, serial)
		    )
			found = handle;

		if (found)
			break;
	}
	libusb_free_device_list(list, 1);

	return found;
}


static int hotplug_callback_fn(libusb_context *ctx, libusb_device *device, libusb_hotplug_event event, void *user_data)
{
	struct ncusb_devwatch_data *d = user_data;
	struct libusb_device_handle *hndl = ncusb_try_device(ctx, device,
							     d->vid, d->pid,
							     d->vendor, d->product, d->serial);

	if (hndl) {
		d->device_found_func(hndl, d->arg);
		/* Thanks, we're good from now on */
		return 1;
	}

	return 0;
}

static void pollfd_added_cb(int fd, short events, void *user_data)
{
	slog(4, SLOG_DEBUG, "usb-helpers: Adding fd %d evts %x to node", fd, events);
	aura_add_pollfds(user_data, fd, events);
}

static void pollfd_removed_cb(int fd, void *user_data)
{
	slog(4, SLOG_DEBUG, "usb-helpers: Removing fd %d from node", fd);
	aura_del_pollfds(user_data, fd);
}

void ncusb_start_descriptor_watching(struct aura_node *node, libusb_context *ctx)
{
	/* Fetch all existing descriptors from the library */
	const struct libusb_pollfd **fds_data = libusb_get_pollfds(ctx);
	const struct libusb_pollfd **fds = fds_data;

	while (*fds != NULL) {
		pollfd_added_cb((*fds)->fd, (*fds)->events, node);
		fds++;
	}
	free(fds_data);
	/* Register a callback */
	libusb_set_pollfd_notifiers(ctx, pollfd_added_cb, pollfd_removed_cb, node);
}

int ncusb_watch_for_device(libusb_context *		ctx,
			   struct ncusb_devwatch_data * dwatch)
{
	return libusb_hotplug_register_callback(ctx, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED,
						LIBUSB_HOTPLUG_ENUMERATE, dwatch->vid, dwatch->pid,
						LIBUSB_HOTPLUG_MATCH_ANY,
						hotplug_callback_fn,
						dwatch, NULL);
}
