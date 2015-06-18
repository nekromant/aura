#include <aura/aura.h>
#include <aura/usb_helpers.h>
#include <libusb.h>
#include <lua.h>
#include <lauxlib.h>


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

int susb_open(struct aura_node *node, va_list ap)
{

	struct libusb_context *ctx;
	struct usb_dev_info *inf = calloc(1, sizeof(*inf));
	const char *conf = va_arg(ap, const char *);
	lua_State* L=lua_open();
	luaL_openlibs(L);
	int ret; 
	ret = libusb_init(&ctx);
	if (ret != 0) 
		return -ENODEV;
	if (!inf)
		return -ENOMEM;

	slog(2, SLOG_INFO, "usbsimple: config file %s", conf);

	inf->ctx = ctx;
	inf->io_buf_size = 256;
	

	int status = luaL_loadfile(L, conf);
	if (status) {
		slog(2, SLOG_INFO, "usbsimple: config file load error");
		return -ENODEV;
	}
	/* TODO: proper error handling */

	lua_pushstring(L, "hello");
	/* Ask Lua to run our little script */
	status = lua_pcall(L, 1, 5, 0);
	if (status) { 
		slog(2, SLOG_INFO, "usbsimple: config file exec error");
		return -EIO;
	}

	stack_dump(L);	

	dbg("config file loaded, syntax ok!");

	
/*
	inf->vid = va_arg(ap, int);
	inf->pid = va_arg(ap, int);
	inf->conf    = va_arg(ap, const char *);
	inf->vendor  = va_arg(ap, const char *);
	inf->product = va_arg(ap, const char *);
	inf->serial  = va_arg(ap, const char *);
*/
	aura_set_transportdata(node, inf);	

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
