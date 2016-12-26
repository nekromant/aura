#include <aura/aura.h>
#include <aura/private.h>
#include <aura/usb_helpers.h>
#include <libusb.h>

struct usb_interrupt_packet {
	uint8_t pending_evts;
} __attribute__((packed));

#define usbdev_is_be(pck) (pck->flags & (1 << 0))

struct usb_info_packet {
	uint8_t		flags;
	uint16_t	num_objs;
	uint16_t	io_buf_size;
} __attribute__((packed));

struct usb_event_packet {
	uint16_t	id;
	char		data[];
} __attribute__((packed));

/*
 * Object Info Packet contains three NULL-terminated strings:
 * name \00 argfmt \00 retfmt \00
 */

enum device_state {
	AUSB_DEVICE_SEARCHING,
	AUSB_DEVICE_INIT,
	AUSB_DEVICE_OPERATIONAL,
	AUSB_DEVICE_FAILING,
	AUSB_DEVICE_RESTART,
	AUSB_DEVICE_STOP
};

/*
 *
 * OFFLINE                    |  ONLINE
 |
 | search -> init -> discover -> operational
 |        |        |             |
 |
 | /\
 | restart
 | /\
 |
 | failing
 |  <-   err   <- err   <-      err
 | Errors reset state to 'searching'

 */

struct usb_dev_info {
	char *				optbuf;
	struct aura_node *		node;
	uint8_t				flags;
	struct aura_export_table *	etbl;

	int				state;
	int				current_object;
	uint16_t			num_objects;
	uint16_t			io_buf_size;
	int				pending;

	struct libusb_context *		ctx;
	libusb_device_handle *		handle;

	struct aura_buffer *		current_buffer;
	unsigned char *			ctrlbuf;
	struct libusb_transfer *	ctransfer;
	bool				cbusy;
	bool				ibusy;
	bool				itransfer_enabled;
	struct libusb_transfer *	itransfer;
	unsigned char			ibuffer[8]; /* static interrupt buffer */

	/* Usb device string */
	struct ncusb_devwatch_data	dev_descr;
	struct aura_timer *timer;
};

enum usb_requests {
	RQ_GET_DEV_INFO,
	RQ_GET_OBJ_INFO,
	RQ_GET_EVENT,
	RQ_PUT_CALL
};


/* Error handling stuff */
#define failing(inf) (inf->state == AUSB_DEVICE_FAILING)

static void usb_panic_and_reset_state(struct aura_node *node)
{
	struct usb_dev_info *inf = aura_get_transportdata(node);
	slog(4, SLOG_DEBUG, "usb: transport failure detected");
	slog(4, SLOG_DEBUG, "usb: pending transfers: %s %s",
	     inf->ibusy ? "interrupt " : "",
	     inf->cbusy ? "control" : "");

	if (inf->state != AUSB_DEVICE_SEARCHING) {
		slog(4, SLOG_DEBUG, "usb: Entering 'failing' state");
		inf->state = AUSB_DEVICE_FAILING;
		if (inf->ibusy) {
			libusb_cancel_transfer(inf->itransfer);
			slog(4, SLOG_DEBUG, "usb: cancelling itransfer");
		}
		if (inf->cbusy) {
			libusb_cancel_transfer(inf->ctransfer);
			slog(4, SLOG_DEBUG, "usb: cancelling ctransfer");
		}
	}

	if ((!inf->ibusy) && (!inf->cbusy)) {
		slog(4, SLOG_DEBUG, "usb: Cleanup done, entering 'restart' state");
		/*
		 * We can't call libusb_close from within a callback
		 * some versions of libusb will hang, we'll close
		 * from main loop
		 */
		/* libusb_close(inf->handle); */
		inf->state = AUSB_DEVICE_RESTART;
	}
}

static void submit_control(struct aura_node *node)
{
	int ret;
	struct usb_dev_info *inf = aura_get_transportdata(node);

	ret = libusb_submit_transfer(inf->ctransfer);
	if (ret != 0) {
		slog(0, SLOG_ERROR, "usb: error submitting control transfer: %s", libusb_error_name(ret));
		usb_panic_and_reset_state(node);
	}
	inf->cbusy = true;
}

static void submit_interrupt(struct aura_node *node)
{
	struct usb_dev_info *inf = aura_get_transportdata(node);
	int ret;

	if (inf->ibusy)
		return;
	ret = libusb_submit_transfer(inf->itransfer);
	inf->ibusy = true;
	if (ret != 0) {
		slog(0, SLOG_ERROR, "usb: error submitting interrupt transfer");
		usb_panic_and_reset_state(node);
	}
}

static int check_interrupt(struct libusb_transfer *transfer)
{
	struct aura_node *node = transfer->user_data;
	struct usb_dev_info *inf = aura_get_transportdata(node);
	int ret = 0;

	inf->ibusy = false;

	if (failing(inf) || (transfer->status != LIBUSB_TRANSFER_COMPLETED)) {
		slog(1, SLOG_ERROR, "Interrupt transfer failed: %s",
			libusb_strerror(transfer->status));
		//usb_panic_and_reset_state(node);
		//ret = -EIO;
	}
	return 0;
}

static int check_control(struct libusb_transfer *transfer)
{
	struct aura_node *node = transfer->user_data;
	struct usb_dev_info *inf = aura_get_transportdata(node);
	int ret = 0;

	inf->cbusy = false;

	if (failing(inf) || (transfer->status != LIBUSB_TRANSFER_COMPLETED)) {
		usb_panic_and_reset_state(node);
		ret = -EIO;
	}
	return ret;
}

/* FixMe: Boundary checking, hardware may be nasty */
static inline char *next(char *nx)
{
	return &nx[strlen(nx) + 1];
}

static void itransfer_enable(struct aura_node *node, bool enable)
{
	struct usb_dev_info *inf = aura_get_transportdata(node);

	inf->itransfer_enabled = enable;
	if (failing(inf)) {
		usb_panic_and_reset_state(node);
		return;
	}

	if (!enable)
		return;

	submit_interrupt(node);
}

static void cb_interrupt(struct libusb_transfer *transfer)
{
	struct aura_node *node = transfer->user_data;
	struct usb_dev_info *inf = aura_get_transportdata(node);

	if (0 != check_interrupt(transfer))
		return;

	struct usb_interrupt_packet *pck = (struct usb_interrupt_packet *)inf->ibuffer;
	inf->pending = (int)pck->pending_evts;
	itransfer_enable(node, inf->itransfer_enabled);
}

static void request_object(struct aura_node *node, int id);
static void cb_parse_object(struct libusb_transfer *transfer)
{
	struct aura_node *node = transfer->user_data;
	struct usb_dev_info *inf = aura_get_transportdata(node);
	char is_method;
	char *name, *afmt, *rfmt;

	if (0 != check_control(transfer))
		return;

	name = (char *)libusb_control_transfer_get_data(transfer);

	#if 1
	 aura_hexdump("info", name, transfer->actual_length);
	#endif

	is_method = *name++;
	afmt = next(name);
	rfmt = next(afmt);

	slog(4, SLOG_DEBUG, "usb: got %s %s / '%s' '%s'",
	     is_method ? "method" : "event",
	     name, afmt, rfmt);

	aura_etable_add(inf->etbl, name,
			is_method ? afmt : NULL,
			rfmt);

	if (inf->current_object == inf->num_objects) {
		slog(4, SLOG_DEBUG, "etable becomes active");
		aura_etable_activate(inf->etbl);
		aura_set_status(node, AURA_STATUS_ONLINE);
		inf->state = AUSB_DEVICE_OPERATIONAL;
		itransfer_enable(node, true);
		return;
	}

	slog(4, SLOG_DEBUG, "Requesting info about obj %d", inf->current_object);
	/* Resubmit the transfer for the next round */
	request_object(node, inf->current_object++);
}

static void request_object(struct aura_node *node, int id)
{
	struct usb_dev_info *inf = aura_get_transportdata(node);
	slog(4, SLOG_DEBUG, "requesting object %d", id);
	libusb_fill_control_setup(inf->ctrlbuf,
				  LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_OTHER | LIBUSB_ENDPOINT_IN,
				  RQ_GET_OBJ_INFO,
				  0, id, inf->io_buf_size - LIBUSB_CONTROL_SETUP_SIZE);
	libusb_fill_control_transfer(inf->ctransfer, inf->handle, inf->ctrlbuf, cb_parse_object, node, 1500);
	submit_control(node);
}

static void cb_got_dev_info(struct libusb_transfer *transfer)
{
	struct aura_node *node = transfer->user_data;
	struct usb_dev_info *inf = aura_get_transportdata(node);
	struct usb_info_packet *pck;
	int newbufsize;
	char *tmp;

	check_control(transfer);
	slog(4, SLOG_DEBUG, "usb: Got info packet from device");
	if (inf->state != AUSB_DEVICE_INIT)
		return; /* Wrong state */

	if (transfer->actual_length < sizeof(struct usb_info_packet)) {
		slog(0, SLOG_ERROR, "usb: short-read on info packet want %d got %d  (API mismatch?)",
		     sizeof(struct usb_info_packet), transfer->actual_length);
		usb_panic_and_reset_state(node);
		return;
	}

	pck = (struct usb_info_packet *)libusb_control_transfer_get_data(transfer);
	aura_set_node_endian(node, usbdev_is_be(pck) ?
			     AURA_ENDIAN_BIG : AURA_ENDIAN_LITTLE);

	newbufsize = pck->io_buf_size + LIBUSB_CONTROL_SETUP_SIZE;
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
		inf->ctrlbuf = (unsigned char *)tmp;
	}
	slog(4, SLOG_DEBUG, "usb: Device has %d objects, now running discovery", inf->num_objects);
	inf->etbl = aura_etable_create(node, inf->num_objects);
	if (!inf->etbl) {
		slog(0, SLOG_ERROR, "usb: etable creation failed");
		aura_panic(node);
	}

	inf->current_object = 0;
	request_object(node, inf->current_object++);
}

static void usb_stop_ops(void *arg)
{
	struct usb_dev_info *inf = arg;
	usb_panic_and_reset_state(inf->node);
}

static void usb_start_ops(struct libusb_device_handle *hndl, void *arg)
{
	/* FixMe: Reading descriptors is synchronos. This is not needed
	 * often, but leaves a possibility of a flaky usb device to
	 * screw up the event processing.
	 * A proper workaround would be manually reading out string descriptors
	 * from a device in an async fasion in the background.
	 */
	int ret;
	struct usb_dev_info *inf = arg;
	struct aura_node *node = inf->node;

	inf->handle = hndl;

	libusb_fill_interrupt_transfer(inf->itransfer, inf->handle, 0x81,
				       inf->ibuffer, 8,
				       cb_interrupt, node, 10000);

	libusb_fill_control_setup(inf->ctrlbuf,
				  LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_OTHER | LIBUSB_ENDPOINT_IN,
				  RQ_GET_DEV_INFO,
				  0, 0, inf->io_buf_size - LIBUSB_CONTROL_SETUP_SIZE);
	libusb_fill_control_transfer(inf->ctransfer, inf->handle, inf->ctrlbuf, cb_got_dev_info, node, 1500);
	ret = libusb_submit_transfer(inf->ctransfer);
	if (ret != 0) {
		libusb_close(inf->handle);
		usb_panic_and_reset_state(inf->node);
		return;
	}

	inf->state = AUSB_DEVICE_INIT; /* Change our state */
	inf->cbusy = true;
	slog(4, SLOG_DEBUG, "usb: Device opened, info packet requested");
};


static void parse_params(struct usb_dev_info *inf)
{
	char *sptr;
	char *tmp;

	tmp = strtok_r(inf->optbuf, ":", &sptr);
	if (!tmp)
		return;
	inf->dev_descr.vid = strtoul(tmp, NULL, 16);

	tmp = strtok_r(NULL, ";", &sptr);
	if (!tmp)
		return;
	inf->dev_descr.pid = strtoul(tmp, NULL, 16);

	tmp = strtok_r(NULL, ";", &sptr);
	if (!tmp)
		return;
	inf->dev_descr.vendor = tmp;

	tmp = strtok_r(NULL, ";", &sptr);
	if (!tmp)
		return;
	inf->dev_descr.product = tmp;

	tmp = strtok_r(NULL, ";", &sptr);
	if (!tmp)
		return;
	inf->dev_descr.serial = tmp;
}

static int usb_open(struct aura_node *node, const char *opts)
{
	int ret;
	struct usb_dev_info *inf = calloc(1, sizeof(*inf));
	if (!inf)
		return -ENOMEM;

	ret = libusb_init(&inf->ctx);
	if (ret != 0)
		goto err_free_inf;


	inf->io_buf_size = 256;
	inf->optbuf = strdup(opts);
	inf->dev_descr.device_found_func = usb_start_ops;
	inf->dev_descr.device_left_func = usb_stop_ops;
	inf->dev_descr.arg = inf;
	inf->node = node;
	parse_params(inf);

	ncusb_start_descriptor_watching(node, inf->ctx);
	aura_set_transportdata(node, inf);

	inf->timer = ncusb_timer_create(node, inf->ctx);
	if (!inf->timer)
		goto err_libusb_exit;

	slog(4, SLOG_INFO, "usb: vid 0x%x pid 0x%x vendor %s product %s serial %s",
	     inf->dev_descr.vid, inf->dev_descr.pid, inf->dev_descr.vendor,
	     inf->dev_descr.product, inf->dev_descr.serial);

	inf->ctrlbuf = malloc(inf->io_buf_size);
	if (!inf->ctrlbuf)
		goto err_free_timer;
	inf->itransfer = libusb_alloc_transfer(0);
	if (!inf->itransfer)
		goto err_free_cbuf;

	inf->ctransfer = libusb_alloc_transfer(0);
	if (!inf->ctransfer)
		goto err_free_int;

	slog(1, SLOG_INFO, "usb: Now looking for a matching device");
	ncusb_watch_for_device(inf->ctx, &inf->dev_descr);
	return 0;

err_free_int:
	libusb_free_transfer(inf->itransfer);
err_free_cbuf:
	free(inf->ctrlbuf);
err_free_timer:
	aura_timer_destroy(inf->timer);
err_libusb_exit:
	libusb_exit(inf->ctx);
err_free_inf:
	free(inf);
	return -ENOMEM;
}

static void usb_close(struct aura_node *node)
{
	struct usb_dev_info *inf = aura_get_transportdata(node);

	inf->itransfer_enabled = false;
	/* Waiting for pending transfers */
	slog(4, SLOG_INFO, "usb: Waiting for transport to close from state %d",
		inf->state);

	usb_panic_and_reset_state(node);

	while (inf->state != AUSB_DEVICE_RESTART)
		libusb_handle_events(inf->ctx);

	slog(4, SLOG_INFO, "usb: Cleaning up...");
	libusb_free_transfer(inf->ctransfer);
	libusb_free_transfer(inf->itransfer);
	free(inf->ctrlbuf);
	if (inf->handle)
		libusb_close(inf->handle);
	libusb_exit(inf->ctx);
	if (inf->optbuf)
		free(inf->optbuf);
	free(inf);
}

static void cb_event_readout_done(struct libusb_transfer *transfer)
{
	struct aura_node *node = transfer->user_data;
	struct usb_dev_info *inf = aura_get_transportdata(node);
	struct usb_event_packet *evt;
	struct aura_buffer *buf = inf->current_buffer;
	struct aura_object *o;

	if (0 != check_control(transfer))
		goto ignore;

	if (transfer->actual_length < sizeof(struct usb_event_packet))
		goto ignore;


	evt = (struct usb_event_packet *)libusb_control_transfer_get_data(transfer);
	o = aura_etable_find_id(node->tbl, evt->id);
	if (!o) {
		slog(0, SLOG_ERROR, "usb: got bogus event id from device %d, resetting", evt->id);
		goto panic;
	}

	if ((transfer->actual_length - LIBUSB_CONTROL_SETUP_SIZE) <
	    (sizeof(struct usb_event_packet) + o->retlen)) {
		slog(0, SLOG_ERROR, "usb: short read for evt %d: %d bytes expected %d got",
		     evt->id, o->retlen + sizeof(struct usb_event_packet),
		     transfer->actual_length);
		goto panic;
	}

	inf->pending--;
	slog(4, SLOG_DEBUG, "Event readout completed, %d bytes, %d evt left",
	     transfer->actual_length, inf->pending);
	buf->object = o;
	/* Position the buffer at the start of the responses */
	buf->pos = LIBUSB_CONTROL_SETUP_SIZE + sizeof(struct usb_event_packet);
	aura_node_write(node, buf);

	return;

panic:
	usb_panic_and_reset_state(node);
ignore:
	aura_buffer_release(buf);
	inf->current_buffer = NULL;
	return;
}

static void submit_event_readout(struct aura_node *node)
{
	struct usb_dev_info *inf = aura_get_transportdata(node);
	struct aura_buffer *buf = aura_buffer_request(node, inf->io_buf_size);

	if (!buf)
		return; /* Nothing bad, we'll try again later */

	slog(0, SLOG_DEBUG, "Starting evt readout, max %d bytes, pending %d",
	     inf->io_buf_size, inf->pending);
	libusb_fill_control_setup((unsigned char *)buf->data,
				  LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_OTHER | LIBUSB_ENDPOINT_IN,
				  RQ_GET_EVENT,
				  0, 0, buf->size - LIBUSB_CONTROL_SETUP_SIZE);
	libusb_fill_control_transfer(inf->ctransfer, inf->handle, (unsigned char *)buf->data,
				     cb_event_readout_done, node, 1500);
	inf->current_buffer = buf;
	submit_control(node);
}

static void cb_call_write_done(struct libusb_transfer *transfer)
{
	struct aura_node *node = transfer->user_data;
	struct usb_dev_info *inf = aura_get_transportdata(node);

	if (0 != check_control(transfer))
		/* Put it back to queue. Core will deal with it later */
		goto requeue;

	if (transfer->actual_length - LIBUSB_CONTROL_SETUP_SIZE < transfer->length) {
		slog(0, SLOG_ERROR, "usb: short-write on call packet want %d got %d  (API mismatch?)",
		     transfer->length, transfer->actual_length);
		usb_panic_and_reset_state(node);
		goto requeue;
	}

	aura_buffer_release(inf->current_buffer);
	inf->current_buffer = NULL;
	slog(4, SLOG_DEBUG, "Call write done");
	return;
requeue:
	aura_requeue_buffer(&node->outbound_buffers, inf->current_buffer);
	inf->current_buffer = NULL;
}


static void submit_call_write(struct aura_node *node, struct aura_buffer *buf)
{
	struct aura_object *o = buf->object;
	struct usb_dev_info *inf = aura_get_transportdata(node);

	slog(4, SLOG_DEBUG, "Writing call %s data to device, %d bytes", o->name, o->arglen);
	inf->current_buffer = buf;
	libusb_fill_control_setup((unsigned char *)buf->data,
				  LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_OTHER | LIBUSB_ENDPOINT_OUT,
				  RQ_PUT_CALL,
				  0, 0, o->arglen);
	libusb_fill_control_transfer(inf->ctransfer, inf->handle,
				     (unsigned char *)buf->data,
				     cb_call_write_done, node, 1500);
	submit_control(node);
}

static void usb_handle_event(struct aura_node *node, enum node_event evt, const struct aura_pollfds *fd)
{
	struct aura_buffer *buf;
	struct usb_dev_info *inf = aura_get_transportdata(node);

	ncusb_handle_events_nonblock_once(node, inf->ctx, inf->timer);

	if (inf->cbusy)
		return;

	if (inf->state == AUSB_DEVICE_RESTART) {
		slog(4, SLOG_DEBUG, "usb: transport offlined, starting to look for a device");
		aura_set_status(node, AURA_STATUS_OFFLINE);
		libusb_close(inf->handle);
		inf->handle = NULL;
		inf->state = AUSB_DEVICE_SEARCHING;
		return;
	}

	if (inf->state == AUSB_DEVICE_OPERATIONAL) {
		if (inf->pending)
			submit_event_readout(node);
		else if ((buf = aura_dequeue_buffer(&node->outbound_buffers)))
			submit_call_write(node, buf);
	}
}

static struct aura_transport usb = {
	.name			= "usb",
	.open			= usb_open,
	.close			= usb_close,
	.handle_event	= usb_handle_event,
	.buffer_overhead	= LIBUSB_CONTROL_SETUP_SIZE, /* Offset for usb SETUP structure */
	.buffer_offset		= LIBUSB_CONTROL_SETUP_SIZE
};

AURA_TRANSPORT(usb);
