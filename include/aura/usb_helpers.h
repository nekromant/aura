#ifndef USB_HELPERS_H
#define USB_HELPERS_H

#include <libusb.h>
void ncusb_print_libusb_transfer(struct libusb_transfer *p_t);
int ncusb_match_string(libusb_device_handle *dev, int index, const char* string);

struct libusb_device_handle *ncusb_find_and_open(struct libusb_context *ctx, 
					  int vendor, int product, 
					  const char *vendor_name, 
					  const char *product_name, 
					  const char *serial);

#endif
