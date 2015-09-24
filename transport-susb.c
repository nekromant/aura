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
	struct aura_node *node;
	uint8_t  flags;
	struct aura_export_table *etbl;

	int state; 
	int current_object; 
	uint16_t num_objects;
	uint16_t io_buf_size;
	int pending;

	struct libusb_context *ctx; 
	libusb_device_handle *handle;
	
	struct aura_buffer *current_buffer;
	struct libusb_transfer* ctransfer;
	bool cbusy; 

	/* Usb device string */
	struct ncusb_devwatch_data dev_descr;
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
	if (ret!= 0) {
		slog(0, SLOG_ERROR, "usb: error submitting control transfer");
		usb_panic_and_reset_state(node);
		return; 
	}
	inf->cbusy=true;
}

static int check_control(struct libusb_transfer *transfer) 
{
	struct aura_node *node = transfer->user_data;
	struct usb_dev_info *inf = aura_get_transportdata(node);
	int ret = 0; 

	inf->cbusy = false; 
		
	if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		usb_panic_and_reset_state(node);
		ret = -EIO;
	}
	return ret; 
}

static int usb_start_ops(struct libusb_device_handle *hndl, void *arg)
{
	/* FixMe: Reading descriptors is synchronos. This is not needed
	 * often, but leaves a possibility of a flaky usb device to 
	 * screw up the event processing.
	 * A proper workaround would be manually reading out string descriptors
	 * from a device in an async fasion in the background. 
	 */
	struct usb_dev_info *inf = arg;
	struct aura_node *node = inf->node; 

	inf->handle = hndl;
	
	inf->state = SUSB_DEVICE_OPERATIONAL;
	slog(4, SLOG_DEBUG, "susb: Device opened and ready to accept calls");
	aura_set_status(node, AURA_STATUS_ONLINE);
	return 0;
};



static void lua_settoken(lua_State *L, const char* name, char t) {
	char tmp[2]; 
	tmp[1]=0x0;
	tmp[0]=t;
	lua_pushstring(L, tmp);
	lua_setglobal(L, name);
}

static char *lua_strfromstack(lua_State *L, int n)
{
	char *ret = NULL;
	if (lua_isstring(L, n))
		ret  = strdup(lua_tostring(L, n));
	return ret;
}

extern int luaopen_auracore (lua_State *L);
static int susb_open(struct aura_node *node, const char *conf)
{

	struct libusb_context *ctx;
	struct usb_dev_info *inf = calloc(1, sizeof(*inf));
	int ret;
	lua_State* L;
 
	if (!inf)
		return -ENOMEM;

	ret = libusb_init(&ctx);
	if (ret != 0) 
		goto err_free_inf;
	inf->ctx = ctx;

	L=luaL_newstate();
	if (!L)
		goto err_free_ctx;

 	inf->ctransfer = libusb_alloc_transfer(0);
	if (!inf->ctransfer)
		goto err_free_lua;

	luaL_openlibs(L);
	luaopen_auracore(L);
	lua_setglobal(L, "aura");

	slog(2, SLOG_INFO, "usbsimple: config file %s", conf);
	ret = luaL_loadfile(L, "lua/aura/conf-loader.lua");
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

	lua_settoken(L, "UINT8",  URPC_U8);
	lua_settoken(L, "UINT16", URPC_U16);
	lua_settoken(L, "UINT32", URPC_U32);
	lua_settoken(L, "UINT64", URPC_U64);

	lua_settoken(L, "SINT8",   URPC_S8);
	lua_settoken(L, "SINT16",  URPC_S16);
	lua_settoken(L, "SINT32",  URPC_S32);
	lua_settoken(L, "SINT64",  URPC_S64);

	lua_settoken(L, "FMT_BIN",  URPC_BIN);

	/* Ask Lua to run our little script */
	ret = lua_pcall(L, 0, 5, 0);
	if (ret) { 
		const char* err = lua_tostring(L, -1);
		slog(0, SLOG_FATAL, "usbsimple: %s", err);
		goto err_free_lua;
	}

	inf->dev_descr.vid = lua_tonumber(L, 1);
	inf->dev_descr.pid = lua_tonumber(L, 2);
	inf->dev_descr.vendor = lua_strfromstack(L, 3);
	inf->dev_descr.product = lua_strfromstack(L, 4);
	inf->dev_descr.serial  = lua_strfromstack(L, 5);
	inf->dev_descr.device_found_func = usb_start_ops;
	inf->dev_descr.arg = inf;
	inf->node = node;
	/* We no not need this state anymore */
	lua_close(L); 
	aura_set_transportdata(node, inf);
	
	ncusb_start_descriptor_watching(node, inf->ctx);
	slog(1, SLOG_INFO, "usb: Now looking for a matching device");
	ncusb_watch_for_device(inf->ctx, &inf->dev_descr);

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
	slog(0, SLOG_INFO, "Closing susb transport");

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

	libusb_exit(inf->ctx);
	free(inf);
}

static void cb_call_done(struct libusb_transfer *transfer)
{
	struct aura_node *node = transfer->user_data;
	struct usb_dev_info *inf = aura_get_transportdata(node);
	struct aura_buffer *buf = inf->current_buffer;

	check_control(transfer);
	/* Put the buffer pointer at the start of the data we've got (if any) */
	aura_buffer_rewind(node, buf);
	aura_queue_buffer(&node->inbound_buffers, buf);
	inf->current_buffer = NULL;
}
static void susb_issue_call(struct aura_node *node, struct aura_buffer *buf) 
{ 
	struct aura_object *o = buf->userdata;
	struct usb_dev_info *inf = aura_get_transportdata(node);
	uint8_t rqtype;
	uint16_t wIndex, wValue, *ptr;


	if (o->ret_fmt)
		rqtype = LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_OUT;
	else
		rqtype = LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_IN;

	aura_buffer_rewind(node, buf);
	ptr = (uint16_t *) &buf->data[buf->pos]; 
	wValue = *ptr++;
	wIndex = *ptr++;

	memmove(&buf->data[buf->pos], ptr, buf->size - LIBUSB_CONTROL_SETUP_SIZE - 2*sizeof(uint16_t));

	/* e.g if device is big endian, but has le descriptors
	 * we have to be extra careful here 
	 */

	if (node->need_endian_swap) { 
		wValue = __swap16(wValue);
		wIndex = __swap16(wValue);
	}
	
	inf->current_buffer = buf; 
	libusb_fill_control_setup((unsigned char *) buf->data, rqtype, o->id, wValue, wIndex, 
				  buf->size - LIBUSB_CONTROL_SETUP_SIZE);

	libusb_fill_control_transfer(inf->ctransfer, inf->handle, 
				     (unsigned char *) buf->data, cb_call_done, node, 10000);
	submit_control(node);	
}

static void susb_loop(struct aura_node *node, const struct aura_pollfds *fd)
{
	struct aura_buffer *buf;
	struct usb_dev_info *inf = aura_get_transportdata(node);
	struct timeval tv = { 
		.tv_sec  = 0,
		.tv_usec = 0
	};

	libusb_handle_events_timeout(inf->ctx, &tv);
	slog(0, SLOG_DEBUG, "susb: loop state %d", inf->state);
	if (inf->cbusy)
		return; 
	
	if (inf->state == SUSB_DEVICE_RESTART) { 
		slog(4, SLOG_DEBUG, "usb: transport offlined, starting to look for a device");
		aura_set_status(node, AURA_STATUS_OFFLINE);
		libusb_close(inf->handle);
		inf->state = SUSB_DEVICE_SEARCHING;
		ncusb_watch_for_device(inf->ctx, &inf->dev_descr);
	} else if (inf->state == SUSB_DEVICE_OPERATIONAL) {   
		buf = aura_dequeue_buffer(&node->outbound_buffers); 
		if (!buf)
			return;
		slog(4, SLOG_DEBUG, "susb: outgoing...");
		susb_issue_call(node, buf);
	}
}

static struct aura_transport tusb = { 
	.name = "simpleusb",
	.open = susb_open,
	.close = susb_close,
	.loop  = susb_loop,
	/* We write wIndex and wValue in the setup part of the packet */ 
	.buffer_overhead = LIBUSB_CONTROL_SETUP_SIZE, 
	.buffer_offset = LIBUSB_CONTROL_SETUP_SIZE
};
AURA_TRANSPORT(tusb);
