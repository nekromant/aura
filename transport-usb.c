#include <aura/aura.h>
#include <aura/usb_helpers.h>
#include <libusb.h>

/* int vid, int pid, char *vendor, char *product, char *serial */


struct usb_interrupt_packet { 
	uint8_t pending_evts; 
} __attribute__((packed));

#define usbdev_is_be(pck) (pck->flags & (1<<0))

struct usb_info_packet { 
	uint8_t  flags;
	uint16_t num_objs;
	uint16_t io_buf_size;
} __attribute__((packed));

/* 
 * Object Info Packet contains three NULL-terminated strings: 
 * name \00 argfmt \00 retfmt \00
 */

enum device_state { 
	AUSB_DEVICE_SEARCHING,
	AUSB_DEVICE_INIT,
	AUSB_DEVICE_DISCOVER,
	AUSB_DEVICE_OPERATIONAL
};


/* 

  OFFLINE                    |  ONLINE  
                             |
 search -> init -> discover -> operational
   |        |        |             |
   |  <-   err   <- err   <-      err
   Errors reset state to 'searching'
 */

struct usb_dev_info { 
	uint8_t  flags;
	struct aura_export_table *etbl;

	int state; 
	int current_object; 
	uint16_t num_objects;
	uint16_t io_buf_size;
	int pending;

	struct libusb_context *ctx; 
	struct libusb_device *dev;
	libusb_device_handle *handle;
	
	struct aura_buffer *current_buffer;
	unsigned char *ctrlbuf;
	struct libusb_transfer* ctransfer;
	bool itransfer_enabled;
	struct libusb_transfer* itransfer;
	unsigned char ibuffer[8]; /* static interrupt buffer */
	bool itransfer_enable;


	int vid;
	int pid;
	const char *vendor;
	const char *product;
	const char *serial;
};

enum usb_requests { 
	RQ_GET_DEV_INFO,
	RQ_GET_OBJ_INFO,
	RQ_GET_EVENT,
	RQ_PUT_CALL
};

static int usb_panic_and_reset_state(struct aura_node *node)
{
	struct usb_dev_info *inf = aura_get_transportdata(node);	
	slog(4, SLOG_DEBUG, "usb: Reverting to 'search' mode");
	aura_set_status(node, AURA_STATUS_OFFLINE);
	inf->state = AUSB_DEVICE_SEARCHING;
}

/* TODO: Boundary checking */
static unsigned char *next(unsigned char *nx)
{
	return &nx[strlen(nx)+1];
}

static void itransfer_enable(struct aura_node *node, bool enable)
{
	struct usb_dev_info *inf = aura_get_transportdata(node);
	int ret; 
	inf->itransfer_enabled = enable;
	if (!enable) 
		return; 
	ret = libusb_submit_transfer(inf->itransfer);
	if (ret!= 0) {
		slog(0, SLOG_ERROR, "usb: error submitting interrupt transfer");
		usb_panic_and_reset_state(node);
	}
}

static void cb_interrupt(struct libusb_transfer *transfer)
{
	struct aura_node *node = transfer->user_data;
	struct usb_dev_info *inf = aura_get_transportdata(node);
	if (transfer->status != LIBUSB_TRANSFER_COMPLETED) 
		usb_panic_and_reset_state(node);

	struct usb_interrupt_packet *pck = (struct usb_interrupt_packet *) inf->ibuffer;
	slog(4, SLOG_DEBUG, "usb: %d evts pending", (int) inf->ibuffer[0]);

	/* resubmit */ 
	itransfer_enable(node, inf->itransfer_enabled);
}


static void cb_parse_object(struct libusb_transfer *transfer)
{
	struct aura_node *node = transfer->user_data;
	struct usb_dev_info *inf = aura_get_transportdata(node);
	int ret; 
	unsigned char *name, *afmt, *rfmt;

	slog(4, SLOG_DEBUG, "usb: Got object info packet from device");
	if (transfer->status != LIBUSB_TRANSFER_COMPLETED) 
		usb_panic_and_reset_state(node);
	ncusb_print_libusb_transfer(transfer);
	name = libusb_control_transfer_get_data(transfer);
	afmt = next(name);
	rfmt = next(afmt);

	aura_etable_add(inf->etbl, name, afmt, rfmt);

	if (inf->current_object == inf->num_objects) { 
		slog(4, SLOG_DEBUG, "etable becomes active");
		aura_etable_activate(inf->etbl);
		aura_set_status(node, AURA_STATUS_ONLINE);
		itransfer_enable(node, true);
		return;
	}

	slog(4, SLOG_DEBUG, "Requesting info about obj %d", inf->current_object);
	/* Resubmit the transfer for the next round */
	libusb_fill_control_setup(inf->ctrlbuf, 
				  LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_IN,
				  RQ_GET_OBJ_INFO,
				  0, inf->current_object++, inf->io_buf_size);
	libusb_fill_control_transfer(inf->ctransfer, inf->handle, inf->ctrlbuf, cb_parse_object, node, 1500);
	ret = libusb_submit_transfer(inf->ctransfer);
	if (ret!= 0) {
		slog(0, SLOG_ERROR, "usb: error submitting discovery transfer");
		usb_panic_and_reset_state(node);
	}
}

static void cb_got_dev_info(struct libusb_transfer *transfer)
{
	struct aura_node *node = transfer->user_data;
	struct usb_dev_info *inf = aura_get_transportdata(node);
	struct usb_info_packet *pck; 
	int newbufsize; 
	char *tmp; 
	int ret;

	slog(4, SLOG_DEBUG, "usb: Got info packet from device");
	if (transfer->status != LIBUSB_TRANSFER_COMPLETED) 
		usb_panic_and_reset_state(node);

	if (transfer->actual_length < sizeof(struct usb_info_packet)) { 
		slog(0, SLOG_ERROR, "usb: short-read on info packet want %d got %d  (API mismatch?)",
		     sizeof(struct usb_info_packet), transfer->actual_length);
		usb_panic_and_reset_state(node);
	}

	pck = libusb_control_transfer_get_data(transfer);	
	aura_set_node_endian(node, usbdev_is_be(pck) ? 
			     AURA_ENDIAN_BIG : AURA_ENDIAN_LITTLE);

	newbufsize = pck->io_buf_size;
	inf->num_objects = pck->num_objs;

	if (newbufsize > inf->io_buf_size) { 
		slog(4, SLOG_DEBUG, "usb: adjusting control buffer size: %d->%d bytes", 
		     inf->io_buf_size, newbufsize);
		tmp = realloc(inf->ctrlbuf, newbufsize);
		if (!tmp) { 
			slog(0, SLOG_ERROR, "Allocation error while adjusting control buffer size");
			aura_panic(node);
		}
		inf->io_buf_size = newbufsize;
		inf->ctrlbuf = tmp;
	}
	slog(4, SLOG_DEBUG, "usb: Device has %d objects, now running discovery", inf->num_objects);
	inf->etbl = aura_etable_create(node, inf->num_objects);
	if (!inf->etbl) 
	{
		slog(0, SLOG_ERROR, "usb: etable creation failed");
		aura_panic(node);
	}

	inf->current_object = 0; 
	libusb_fill_control_setup(inf->ctrlbuf, 
				  LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_IN,
				  RQ_GET_OBJ_INFO,
				  0, inf->current_object++, inf->io_buf_size);
	libusb_fill_control_transfer(inf->ctransfer, inf->handle, inf->ctrlbuf, cb_parse_object, node, 1500);
	ret = libusb_submit_transfer(inf->ctransfer);
	if (ret!= 0) {
		slog(0, SLOG_ERROR, "usb: error submitting discovery transfer");
		usb_panic_and_reset_state(node);
	}

}

static void usb_try_open_device(struct aura_node *node)
{
	/* FixMe: Reading descriptors is synchronos. This is not needed
	 * often, but leaves a possibility of a flaky usb device to 
	 * screw up the event processing.
	 * A proper workaround would be manually reading out string descriptors
	 * from a device in an async fasion in the background. 
	 */
	int ret;
	struct usb_dev_info *inf = aura_get_transportdata(node);
	inf->dev = ncusb_find_dev(inf->ctx, inf->vid, inf->pid,
				  inf->vendor,
				  inf->product,
				  inf->serial);
	if (!inf->dev) 
		return; /* Not this time */
	ret = libusb_open(inf->dev, &inf->handle);
	if (ret != 0) {
		inf->dev = NULL;
		return; /* Screw it! */
	}
	
	libusb_fill_interrupt_transfer(inf->itransfer, inf->handle, 0x81,
				       inf->ibuffer, 8, 
				       cb_interrupt, node, 10000);

	libusb_fill_control_setup(inf->ctrlbuf, 
				  LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_IN,
				  RQ_GET_DEV_INFO,
				  0, 0, inf->io_buf_size);
	libusb_fill_control_transfer(inf->ctransfer, inf->handle, inf->ctrlbuf, cb_got_dev_info, node, 1500);
	ret = libusb_submit_transfer(inf->ctransfer);
	if (ret!= 0) 
		goto err; 
	inf->state = AUSB_DEVICE_INIT; /* Change our state */
	slog(4, SLOG_DEBUG, "usb: Device opened, info packet requested");
	return;
err:
	libusb_close(inf->handle);
	return;
};

int usb_open(struct aura_node *node, va_list ap)
{
	struct libusb_context *ctx;
	struct usb_dev_info *inf = calloc(1, sizeof(*inf));
	int ret; 

	slog(0, SLOG_INFO, "usb: Opening usb transport");
	ret = libusb_init(&ctx);
	if (ret != 0) 
		return -ENODEV;
	if (!inf)
		return -ENOMEM;

	inf->ctx = ctx;
	inf->io_buf_size = 256;
	inf->vid = va_arg(ap, int);
	inf->pid = va_arg(ap, int);
	inf->vendor  = va_arg(ap, const char *);
	inf->product = va_arg(ap, const char *);
	inf->serial  = va_arg(ap, const char *);
	aura_set_transportdata(node, inf);	

	inf->ctrlbuf   = malloc(inf->io_buf_size);
	if (!inf->ctrlbuf)
		goto err_free_inf;
	inf->itransfer = libusb_alloc_transfer(0);
	if (!inf->itransfer)
		goto err_free_cbuf;

 	inf->ctransfer = libusb_alloc_transfer(0);
	if (!inf->ctransfer)
		goto err_free_int;
	
	usb_try_open_device(node); /* Try it! */
	if (inf->state == AUSB_DEVICE_SEARCHING)
		slog(0, SLOG_WARN, "usb: Device not found, will try again later");
	
	return 0;
	
err_free_int:
	libusb_free_transfer(inf->itransfer);
err_free_inf:
	free(inf);
err_free_cbuf:
	return -ENOMEM;
}

void usb_close(struct aura_node *node)
{
	struct usb_dev_info *inf = aura_get_transportdata(node);
	libusb_exit(inf->ctx);
	slog(0, SLOG_INFO, "usb: Closing transport");
}


void submit_event_readout(struct aura_node *node)
{
	
}

void submit_call_write(struct aura_node *node, struct aura_buffer *buf)
{
	
}

void usb_loop(struct aura_node *node)
{
	struct aura_buffer *buf;
	struct aura_object *o;
	struct usb_dev_info *inf = aura_get_transportdata(node);

	libusb_handle_events(inf->ctx);
	if (inf->state == AUSB_DEVICE_SEARCHING) {
		usb_try_open_device(node);
		return;
	} else if ((inf->state == AUSB_DEVICE_OPERATIONAL) && (!inf->busy))
	{
		if (inf->pending) 
			submit_event_readout(node);
		
	}

	
/*
	while(1) { 
		buf = aura_dequeue_buffer(&node->outbound_buffers); 
		if (!buf)
			break;
		o = buf->userdata;
		slog(0, SLOG_DEBUG, "Dequeued/requeued obj id %d (%s)", o->id, o->name);
		aura_queue_buffer(&node->inbound_buffers, buf);
	}
*/
}

static struct aura_transport usb = { 
	.name = "usb",
	.open = usb_open,
	.close = usb_close,
	.loop  = usb_loop,
	.buffer_overhead = 8, /* Offset for usb SETUP structure */  
	.buffer_offset = 8
};
AURA_TRANSPORT(usb);
