#include <aura/aura.h>
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

	int vid;
	int pid;
	char *vendor;
	char *product;
	char *serial;
	char *conf;
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

static void usb_try_open_device(struct aura_node *node)
{
	/* FixMe: Reading descriptors is synchronos. This is not needed
	 * often, but leaves a possibility of a flaky usb device to 
	 * screw up the event processing.
	 * A proper workaround would be manually reading out string descriptors
	 * from a device in an async fasion in the background. 
	 */
	struct usb_dev_info *inf = aura_get_transportdata(node);
	inf->handle = ncusb_find_and_open(inf->ctx, inf->vid, inf->pid,
					  inf->vendor,
					  inf->product,
					  inf->serial);

	if (!inf->handle)
		return; /* Not this time */
	
	inf->state = SUSB_DEVICE_OPERATIONAL;
	slog(4, SLOG_DEBUG, "susb: Device opened and ready to accept calls");
	aura_set_status(node, AURA_STATUS_ONLINE);
	return;
};



static void lua_settoken(lua_State *L, const char* name, char t) {
	char tmp[2]; 
	tmp[1]=0x0;
	tmp[0]=t;
	lua_pushstring(L, tmp);
	lua_setglobal(L, name);
}

char *lua_strfromstack(lua_State *L, int n)
{
	char *ret = NULL;
	if (lua_isstring(L, n))
		ret  = strdup(lua_tostring(L, n));
	return ret;
}

int luaopen_auracore (lua_State *L);
int susb_open(struct aura_node *node, va_list ap)
{

	struct libusb_context *ctx;
	struct usb_dev_info *inf = calloc(1, sizeof(*inf));
	const char *conf = va_arg(ap, const char *);
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
	lua_pop(L, 1);

	slog(2, SLOG_INFO, "usbsimple: config file %s", conf);
	ret = luaL_loadfile(L, "lua/conf-loader.lua");
	if (ret) {
		slog(2, SLOG_INFO, "usbsimple: config file load error");
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

	/* Ask Lua to run our little script */
	ret = lua_pcall(L, 0, 5, 0);
	if (ret) { 
		const char* err = lua_tostring(L, -1);
		slog(0, SLOG_FATAL, "usbsimple: %s", err);
		goto err_free_lua;
	}

	inf->vid = lua_tonumber(L, 1);
	inf->pid = lua_tonumber(L, 2);
	inf->vendor = lua_strfromstack(L, 3);
	inf->product = lua_strfromstack(L, 4);
	inf->serial  = lua_strfromstack(L, 5);
	/* We no not need this state anymore */
	lua_close(L); 
	aura_set_transportdata(node, inf);	
	usb_try_open_device(node);
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

void susb_close(struct aura_node *node)
{
	struct usb_dev_info *inf = aura_get_transportdata(node);
	slog(0, SLOG_INFO, "Closing susb transport");

	while (inf->cbusy) 
		libusb_handle_events(inf->ctx);

	if (inf->vendor)
		free(inf->vendor);
	if (inf->product)
		free(inf->product);
	if (inf->serial)
		free(inf->serial);

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
	if (!o->ret_fmt)
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

void susb_loop(struct aura_node *node)
{
	struct aura_buffer *buf;
	struct usb_dev_info *inf = aura_get_transportdata(node);
	struct timeval tv = { 
		.tv_sec  = 0,
		.tv_usec = 0
	};

	libusb_handle_events_timeout(inf->ctx, &tv);

	if (inf->cbusy)
		return; 
	
	if (inf->state == SUSB_DEVICE_RESTART) { 
		slog(4, SLOG_DEBUG, "usb: transport offlined, starting to look for a device");
		aura_set_status(node, AURA_STATUS_OFFLINE);
		libusb_close(inf->handle);
		inf->state = SUSB_DEVICE_SEARCHING;
	} else if (inf->state == SUSB_DEVICE_SEARCHING) {
		usb_try_open_device(node);
		return;
	} else if (inf->state == SUSB_DEVICE_OPERATIONAL) {   
		buf = aura_dequeue_buffer(&node->outbound_buffers); 
		if (!buf)
			return;
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
