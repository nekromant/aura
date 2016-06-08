#include <aura/aura.h>
#include <aura/private.h>
#include <aura/usb_helpers.h>
#include <libusb.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>


enum device_state {
	SUSB_DEVICE_SEARCHING=0,
	SUSB_DEVICE_OPERATIONAL,
	SUSB_DEVICE_RESTART
};

struct usb_dev_info {
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
	struct libusb_transfer *	ctransfer;
	bool				cbusy;

	/* Usb device string */
	struct ncusb_devwatch_data	dev_descr;
	struct aura_timer *timer;
};

static void usb_panic_and_reset_state(struct aura_node *node)
{
	struct usb_dev_info *inf = aura_get_transportdata(node);

	if (inf->cbusy)
		BUG(node, "susb: Failing with busy ctransfer");

	inf->state = SUSB_DEVICE_RESTART;
}

static void submit_control(struct aura_node *node)
{
	int ret;
	struct usb_dev_info *inf = aura_get_transportdata(node);

	ret = libusb_submit_transfer(inf->ctransfer);
	if (ret != 0) {
		slog(0, SLOG_ERROR, "usb: error submitting control transfer");
		usb_panic_and_reset_state(node);
		return;
	}
	inf->cbusy = true;
}

static int check_control(struct libusb_transfer *transfer)
{
	struct aura_node *node = transfer->user_data;
	struct usb_dev_info *inf = aura_get_transportdata(node);
	int ret = 0;

	inf->cbusy = false;

	if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		slog(0, SLOG_ERROR, "usb: error completing control transfer");
		ncusb_print_libusb_transfer(transfer);
		usb_panic_and_reset_state(node);
		ret = -EIO;
	}
	return ret;
}

static void usb_stop_ops(void *arg)
{
	struct usb_dev_info *inf = arg;
	usb_panic_and_reset_state(inf->node);
	slog(2, SLOG_INFO, "susb: Device disconnect detected!");
}

static void usb_start_ops(struct libusb_device_handle *hndl, void *arg)
{
	/* FixMe: Reading descriptors is synchronos. This is not needed
	 * often, but leaves a possibility of a flaky usb device to
	 * screw up the event processing.
	 * A proper workaround would be manually reading out string descriptors
	 * from a device in an async fasion in the background.
	 */
	struct usb_dev_info *inf = arg;
	inf->handle = hndl;

	inf->state = SUSB_DEVICE_OPERATIONAL;

	slog(2, SLOG_INFO, "susb: Device opened and ready to accept calls");
	return;
};



static void lua_settoken(lua_State *L, const char *name, char t)
{
	char tmp[2];

	tmp[1] = 0x0;
	tmp[0] = t;
	lua_pushstring(L, tmp);
	lua_setglobal(L, name);
}

static char *lua_strfromstack(lua_State *L, int n)
{
	char *ret = NULL;

	if (lua_isstring(L, n))
		ret = strdup(lua_tostring(L, n));
	return ret;
}

extern int luaopen_auracore(lua_State *L);
static int susb_open(struct aura_node *node, const char *conf)
{
	struct libusb_context *ctx;
	struct usb_dev_info *inf = calloc(1, sizeof(*inf));
	int ret;
	lua_State *L;

	if (!inf)
		return -ENOMEM;

	ret = libusb_init(&ctx);
	if (ret != 0)
		goto err_free_inf;
	inf->ctx = ctx;

	L = luaL_newstate();
	if (!L)
		goto err_free_ctx;

	inf->ctransfer = libusb_alloc_transfer(0);
	if (!inf->ctransfer)
		goto err_free_lua;

	luaL_openlibs(L);
	luaopen_auracore(L);
	lua_setglobal(L, "aura");

	slog(2, SLOG_INFO, "usbsimple: config file %s", conf);

	const char scr[] = "return require(\"aura/conf-loader\")\n";
	ret = luaL_loadbuffer(L, scr, strlen(scr), "ldr");
	lua_call(L, 0, 1);
	if (ret) {
		slog(0, SLOG_ERROR, lua_tostring(L, -1));
		slog(0, SLOG_ERROR, "usbsimple: config file load error");
		goto err_free_ct;
	}

	lua_pushstring(L, conf);
	lua_setglobal(L, "simpleconf");

	lua_pushlightuserdata(L, node);
	lua_setglobal(L, "node");

	/*  We need to push all format tokens to lua.
	 *  We do it here to be always in sync with format.h
	 */

	lua_settoken(L, "UINT8", URPC_U8);
	lua_settoken(L, "UINT16", URPC_U16);
	lua_settoken(L, "UINT32", URPC_U32);
	lua_settoken(L, "UINT64", URPC_U64);

	lua_settoken(L, "SINT8", URPC_S8);
	lua_settoken(L, "SINT16", URPC_S16);
	lua_settoken(L, "SINT32", URPC_S32);
	lua_settoken(L, "SINT64", URPC_S64);

	lua_settoken(L, "FMT_BIN", URPC_BIN);

	ret = lua_pcall(L, 0, 6, 0);
	if (ret) {
		const char *err = lua_tostring(L, -1);
		slog(0, SLOG_FATAL, "usbsimple: %s", err);
		goto err_free_ct;
	}

	inf->dev_descr.vid = lua_tonumber(L, -6);
	inf->dev_descr.pid = lua_tonumber(L, -5);
	inf->dev_descr.vendor = lua_strfromstack(L, -4);
	inf->dev_descr.product = lua_strfromstack(L, -3);
	inf->dev_descr.serial = lua_strfromstack(L, -2);
	inf->etbl = lua_touserdata(L, -1);

	inf->dev_descr.device_found_func = usb_start_ops;
	inf->dev_descr.device_left_func = usb_stop_ops;
	inf->dev_descr.arg = inf;
	inf->node = node;

	inf->timer = ncusb_timer_create(node, inf->ctx);
	if (!inf->timer)
		goto err_free_ct;

	/* We no not need this state anymore */
	lua_close(L);
	aura_set_transportdata(node, inf);

	ncusb_watch_for_device(inf->ctx, &inf->dev_descr);
	ncusb_start_descriptor_watching(node, inf->ctx);
	slog(1, SLOG_INFO, "usb: Now looking for a device %x:%x %s/%s/%s",
	     inf->dev_descr.vid, inf->dev_descr.pid,
	     inf->dev_descr.vendor, inf->dev_descr.product, inf->dev_descr.serial);

	return 0;
err_free_ct:
	libusb_free_transfer(inf->ctransfer);
err_free_lua:
	lua_close(L);
err_free_ctx:
	libusb_exit(inf->ctx);
err_free_inf:
	free(inf);
	return -ENOMEM;
}

static void susb_close(struct aura_node *node)
{
	struct usb_dev_info *inf = aura_get_transportdata(node);

	slog(3, SLOG_INFO, "Closing susb transport");

	while (inf->cbusy)
		libusb_handle_events(inf->ctx);

	if (inf->dev_descr.vendor)
		free(inf->dev_descr.vendor);
	if (inf->dev_descr.product)
		free(inf->dev_descr.product);
	if (inf->dev_descr.serial)
		free(inf->dev_descr.serial);

	libusb_free_transfer(inf->ctransfer);

	if (inf->handle)
		libusb_close(inf->handle);

	/* If we've created etable but have NOT yet activated it
	 *	we have to take care and free it ourselves
	 */
	if (inf->etbl && node->tbl != inf->etbl)
		aura_etable_destroy(inf->etbl);

	libusb_exit(inf->ctx);
	free(inf);
}

static void cb_call_done(struct libusb_transfer *transfer)
{
	struct aura_node *node = transfer->user_data;
	struct usb_dev_info *inf = aura_get_transportdata(node);
	struct aura_buffer *buf;

	/* Put the buffer pointer at the start of the data we've got */
	/* It only makes sense when we succeed */
	if (0 == check_control(transfer)) {
		slog(4, SLOG_DEBUG, "Requeuing!");
		buf = aura_node_read(node);
		aura_node_write(node, buf);
		inf->current_buffer = NULL;
	}
}

static void susb_issue_call(struct aura_node *node, struct aura_buffer *buf)
{
	struct aura_object *o = buf->object;
	struct usb_dev_info *inf = aura_get_transportdata(node);
	uint8_t rqtype;
	uint16_t wIndex, wValue, *ptr;
	size_t datalen; /*Actual data in packet, save for setup */

	if (o->retlen) {
		rqtype = LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_IN;
		datalen = o->retlen;
	} else {
		datalen = o->arglen - 2 * sizeof(uint16_t);
		rqtype = LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_OUT;
	}
	ptr = (uint16_t *)&buf->data[buf->pos];
	wValue = *ptr++;
	wIndex = *ptr++;

	memmove(&buf->data[buf->pos], ptr, datalen);

	/*
	 * VUSB-based devices (or libusb?) do not seem to play nicely when we have the
	 * setup packet with no data. Transfers may fail instantly.
	 * A typical run at my box gives:
	 *
	 * 9122 succeeded, 878 failed total 10000
	 *
	 * See tests-transports/test-susb-stability-none
	 * Adding just one byte of data to the packet fix the issue.
	 * Posible workarounds are:
	 * 1. Retry N times
	 * 2. Add just one dummy byte for the transfer
	 * Since nobody reported this workaround breaking support for their
	 * hardware - we'll do it the second way. For now.
	 */

	if (!datalen)
		datalen++; /* buffer's big enough anyway */

	/* e.g if device is big endian, but has le descriptors
	 * we have to be extra careful here
	 */

	if (node->need_endian_swap) {
		wValue = __swap16(wValue);
		wIndex = __swap16(wValue);
	}

	inf->current_buffer = buf;
	libusb_fill_control_setup((unsigned char *)buf->data, rqtype, o->id, wValue, wIndex,
				  datalen);
	libusb_fill_control_transfer(inf->ctransfer, inf->handle,
				     (unsigned char *)buf->data, cb_call_done, node, 4000);
	submit_control(node);
}

static void susb_handle_event(struct aura_node *node, enum node_event evt, const struct aura_pollfds *fd)
{
	struct aura_buffer *buf;
	struct usb_dev_info *inf = aura_get_transportdata(node);


	ncusb_handle_events_nonblock_once(node, inf->ctx, inf->timer);

	if (inf->cbusy)
		return;

	if (inf->state == SUSB_DEVICE_RESTART) {
		slog(4, SLOG_DEBUG, "usb: transport offlined, starting to look for a device");
		aura_set_status(node, AURA_STATUS_OFFLINE);
		libusb_close(inf->handle);
		inf->handle = NULL;
		inf->state = SUSB_DEVICE_SEARCHING;
		ncusb_watch_for_device(inf->ctx, &inf->dev_descr);
	} else if (inf->state == SUSB_DEVICE_OPERATIONAL) {
		if (inf->etbl) {
			/* Hack: Since libusb tends to send and receive data in one buffer,
			 * we need to adjust argument buffer to fit in return values as well.
			 * It helps us to avoid needless copying.
			 */
			int i;
			for (i = 0; i < inf->etbl->next; i++) {
				struct aura_object *tmp;
				tmp = &inf->etbl->objects[i];
				tmp->arglen += tmp->retlen;
			}
			aura_etable_activate(inf->etbl);
			inf->etbl = NULL;
		}
		aura_set_status(node, AURA_STATUS_ONLINE);
		buf = aura_peek_buffer(&node->outbound_buffers);
		if (!buf)
			return;
		susb_issue_call(node, buf);
	}
}

static struct aura_transport tusb = {
	.name			= "simpleusb",
	.open			= susb_open,
	.close			= susb_close,
	.handle_event	= susb_handle_event,
	/* We write wIndex and wValue in the setup part of the packet */
	.buffer_overhead	= LIBUSB_CONTROL_SETUP_SIZE,
	.buffer_offset		= LIBUSB_CONTROL_SETUP_SIZE
};

AURA_TRANSPORT(tusb);
