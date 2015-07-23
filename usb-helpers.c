#include <aura/aura.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <libusb.h>
#include <aura/usb_helpers.h>


void ncusb_print_libusb_transfer(struct libusb_transfer *p_t)
{	
	if ( NULL == p_t){
		slog(0, SLOG_INFO, "No libusb_transfer...");
	}
	else {
		slog(0, SLOG_INFO, "libusb_transfer structure:");
		slog(0, SLOG_INFO, "flags =%x ", p_t->flags);
		slog(0, SLOG_INFO, "endpoint=%x ", p_t->endpoint);
		slog(0, SLOG_INFO, "type =%x ", p_t->type);
		slog(0, SLOG_INFO, "timeout =%d ", p_t->timeout);
// length, and buffer are commands sent to the device
		slog(0, SLOG_INFO, "length =%d ", p_t->length);
		slog(0, SLOG_INFO, "actual_length =%d ", p_t->actual_length);
		slog(0, SLOG_INFO, "buffer =%p ", p_t->buffer);

	}
	return;
}


int ncusb_match_string(libusb_device_handle *dev, int index, const char* string)
{
	unsigned char tmp[256];
	libusb_get_string_descriptor_ascii(dev, index, tmp, 256);
	dbg("cmp idx %d str %s vs %s", index, tmp, string);
	if (string == NULL)
		return 1; /* NULL matches anything */
	return (strcmp(string, (char*) tmp)==0);
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
	int err = 0;

	if (cnt < 0){
		slog(0, SLOG_ERROR, "no usb devices found" );
		return NULL;
	}

	for(i = 0; i < cnt; i++) {
		libusb_device *device = list[i];
		struct libusb_device_descriptor desc;
		libusb_device_handle *handle;	
		err = libusb_open(device, &handle);
		if (err) 
			continue;
		
		int r = libusb_get_device_descriptor( device, &desc );	
		if (r) {
			libusb_close(handle);
			continue;
		}

		if ( desc.idVendor == vendor && desc.idProduct == product &&
		     ncusb_match_string(handle, desc.iManufacturer, vendor_name) &&
		     ncusb_match_string(handle, desc.iProduct,      product_name) &&
		     ncusb_match_string(handle, desc.iSerialNumber, serial)
			) 
		{
			found = handle;
		}

		if (found) 
			break;
		
	}
	libusb_free_device_list(list, 1);

	return found;
}

int ncusb_watch_for_device(struct libusb_contex *ctx, 
			   int vid, 
			   int pid, 
			   int (*openfunc)(void *arg),
			   void *arg)
{
	
}
