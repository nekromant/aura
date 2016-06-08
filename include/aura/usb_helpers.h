#ifndef USB_HELPERS_H
#define USB_HELPERS_H

#include <libusb.h>
#include <aura/timer.h>

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
	struct aura_timer *tm;
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

void ncusb_handle_events_nonblock_once(struct aura_node *	node,
				       struct libusb_context *	ctx,
				       struct aura_timer *	tm);

struct aura_timer *ncusb_timer_create(struct aura_node *node, struct libusb_context *ctx);
#endif
