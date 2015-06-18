#include <aura/aura.h>
#include <aura/usb_helpers.h>
#include <libusb.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>



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
	const char *vendor;
	const char *product;
	const char *serial;
	const char *conf;
};

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
	inf->handle = ncusb_find_and_open(inf->ctx, inf->vid, inf->pid,
					  inf->vendor,
					  inf->product,
					  inf->serial);

	if (!inf->handle)
		return; /* Not this time */
	
	slog(4, SLOG_DEBUG, "susb: Device opened, ready to accept calls");
	return;
};

static void stack_dump (lua_State *L) {
	int i=lua_gettop(L);
	printf(" ----------------  Stack Dump ----------------\n" );
	while(  i   ) {
		int t = lua_type(L, i);
		switch (t) {
		case LUA_TSTRING:
			printf("%d:`%s'\n", i, lua_tostring(L, i));
			break;
		case LUA_TBOOLEAN:
			printf("%d: %s\n",i,lua_toboolean(L, i) ? "true" : "false");
			break;
		case LUA_TNUMBER:
			printf("%d: %g\n",  i, lua_tonumber(L, i));
			break;
		default: printf("%d: %s\n", i, lua_typename(L, t)); break;
		}
		i--;
	}
	printf("--------------- Stack Dump Finished ---------------\n" );
}


static void lua_settoken(lua_State *L, const char* name, char t) {
	char tmp[2]; 
	tmp[1]=0x0;
	tmp[0]=t;
	lua_pushstring(L, tmp);
	lua_setglobal(L, name);
}


int luaopen_auracore (lua_State *L);
int susb_open(struct aura_node *node, va_list ap)
{

	struct libusb_context *ctx;
	struct usb_dev_info *inf = calloc(1, sizeof(*inf));
	const char *conf = va_arg(ap, const char *);
	lua_State* L=lua_open();
	luaL_openlibs(L);
	luaopen_auracore(L);
	lua_pop(L, 1);

	int ret; 
	ret = libusb_init(&ctx);
	if (ret != 0) 
		return -ENODEV;
	if (!inf)
		return -ENOMEM;

	slog(2, SLOG_INFO, "usbsimple: config file %s", conf);

	inf->ctx = ctx;
	inf->io_buf_size = 256;
	

	int status = luaL_loadfile(L, "lua/conf-loader.lua");
	if (status) {
		slog(2, SLOG_INFO, "usbsimple: config file load error");
		return -ENODEV;
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
	status = lua_pcall(L, 0, 5, 0);
	if (status) { 
		const char* err = lua_tostring(L, -1);
		slog(0, SLOG_FATAL, "usbsimple: %s", err);
		return -EIO;
	}

	dbg("config file loaded, syntax ok!");

	inf->vid = lua_tonumber(L, 1);
	inf->pid = lua_tonumber(L, 2);
	inf->vendor  = strdup(lua_tostring(L, 3));
	inf->product = strdup(lua_tostring(L, 4));
	inf->serial  = strdup(lua_tostring(L, 5));
	/* We no not need this state anymore */
	lua_close(L); 
	aura_set_transportdata(node, inf);	
	usb_try_open_device(node);
	aura_set_status(node, AURA_STATUS_ONLINE);
	return 0;
}

void susb_close(struct aura_node *node)
{
	slog(0, SLOG_INFO, "Closing susb transport");
}

void susb_loop(struct aura_node *node)
{
	struct aura_buffer *buf;
	struct aura_object *o;
	
	while(1) { 
		buf = aura_dequeue_buffer(&node->outbound_buffers); 
		if (!buf)
			break;
		o = buf->userdata;
		slog(0, SLOG_DEBUG, "Dequeued/requeued obj id %d (%s)", o->id, o->name);
		aura_queue_buffer(&node->inbound_buffers, buf);
	}
}

static struct aura_transport tusb = { 
	.name = "simpleusb",
	.open = susb_open,
	.close = susb_close,
	.loop  = susb_loop,
	.buffer_overhead = LIBUSB_CONTROL_SETUP_SIZE, 
	.buffer_offset = LIBUSB_CONTROL_SETUP_SIZE
};
AURA_TRANSPORT(tusb);
