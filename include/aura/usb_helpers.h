#ifndef USB_HELPERS_H
#define USB_HELPERS_H

#include <libusb.h>
void ncusb_print_libusb_transfer(struct libusb_transfer *p_t);
int ncusb_match_string(libusb_device_handle *dev, int index, const char* string);

struct ncusb_devwatch_data {
	void (*device_found_func)(struct libusb_device_handle *, void *arg);
	void (*device_left_func)(void *arg);
	struct libusb_device *current_device;
	void *arg;
	int vid;
	int pid;
	char *vendor;
	char *product;
	char *serial;
};

int ncusb_watch_for_device(libusb_context *ctx,
			   struct ncusb_devwatch_data* dwatch);

struct libusb_device_handle *ncusb_try_device(struct libusb_context *ctx,
					      libusb_device *device,
					      int vid, int pid,
					      const char *vendor_name,
					      const char *product_name,
					      const char *serial);

struct libusb_device_handle *ncusb_find_and_open(struct libusb_context *ctx,
					  int vendor, int product,
					  const char *vendor_name,
					  const char *product_name,
					  const char *serial);

int ncusb_watch_for_device(libusb_context *ctx,
			   struct ncusb_devwatch_data* dwatch);
void ncusb_start_descriptor_watching(struct aura_node *node, libusb_context *ctx);

#endif
