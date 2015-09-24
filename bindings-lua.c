#define LUA_LIB

#include <aura/aura.h>
#include <aura/private.h>
#include <search.h>
#include <lua.h>
#include <lauxlib.h>

#define REF_NODE_CONTAINER (1<<0)
#define REF_STATUS_CB      (1<<1)
#define REF_ETABLE_CB      (1<<2)
#define REF_EVENT_CB       (1<<3)

#define TRACE_BCALLS

#ifdef TRACE_BCALLS
#define TRACE() slog(0, SLOG_DEBUG, "Bindings call: %s", __func__)
#else 
#define TRACE()
#endif



static int l_close(lua_State *L)
{
	struct aura_node *node; 
	struct lua_bindingsdata *bdata;

	TRACE(); 	
	aura_check_args(L, 1);
	if (!lua_islightuserdata(L, 1)) {
		aura_typeerror(L, 1, "ludata");
	}
	node = lua_touserdata(L, 1);
	bdata = aura_get_userdata(node);

	/* Clear up references we've set up so far*/
	
	if (bdata->refs & REF_NODE_CONTAINER)
		luaL_unref(L, LUA_REGISTRYINDEX, bdata->node_container);

	if (bdata->refs & REF_STATUS_CB) { 
		luaL_unref(L, LUA_REGISTRYINDEX, bdata->status_changed_ref);
		luaL_unref(L, LUA_REGISTRYINDEX, bdata->status_changed_arg_ref);
	}

	if (bdata->refs & REF_ETABLE_CB) { 
		luaL_unref(L, LUA_REGISTRYINDEX, bdata->etable_changed_ref);
		luaL_unref(L, LUA_REGISTRYINDEX, bdata->etable_changed_arg_ref);
	}

	if (bdata->refs & REF_EVENT_CB) { 
		luaL_unref(L, LUA_REGISTRYINDEX, bdata->inbound_event_ref);
		luaL_unref(L, LUA_REGISTRYINDEX, bdata->inbound_event_arg_ref);
	}

	free(bdata);
	aura_close(node);

	return 0;
}

static int l_handle_events(lua_State *L)
{
	struct aura_eventloop *loop; 
	int timeout = -1; 

	TRACE();
	aura_check_args(L, 1);

	if (!lua_islightuserdata(L, 1)) {
		aura_typeerror(L, 1, "ludata");
	}

	loop = lua_touserdata(L, 1);

	if (lua_gettop(L) == 2) { 
		timeout = lua_tonumber(L, 2);
		aura_handle_events_timeout(loop, timeout);
	} else { 
		aura_handle_events_forever(loop);
	}

	return 0;
}

/* Call the status changed callback. 
   bdata->status_changed_ref(bdata->status_changed_arg_ref, newstatus)
*/
static void status_cb(struct aura_node *node, int newstatus, void *arg)
{
	struct lua_bindingsdata *bdata = arg; 
	lua_State *L = bdata->L;
	bdata->refs |= REF_STATUS_CB;
	lua_rawgeti(L, LUA_REGISTRYINDEX, bdata->status_changed_ref);
	lua_rawgeti(L, LUA_REGISTRYINDEX, bdata->node_container);
	lua_pushnumber(L, newstatus);
	lua_rawgeti(L, LUA_REGISTRYINDEX, bdata->status_changed_arg_ref);
	lua_call(L, 3, 0);
}

/* 
 * Call the status changed callback. 
 * bdata->status_changed_ref(bdata->status_changed_arg_ref, newstatus)
 */
static void etable_cb(struct aura_node *node, 
		      struct aura_export_table *old, 
		      struct aura_export_table *new, 
		      void *arg)
{
	struct lua_bindingsdata *bdata = arg; 
	lua_State *L = bdata->L;

	lua_rawgeti(L, LUA_REGISTRYINDEX, bdata->etable_changed_ref);
	lua_rawgeti(L, LUA_REGISTRYINDEX, bdata->node_container);
	lua_push_etable(L, old);
	lua_push_etable(L, new);
	lua_rawgeti(L, LUA_REGISTRYINDEX, bdata->etable_changed_arg_ref);
	lua_call(L, 4, 0);
}

/* All events get delivered here */
static void event_cb(struct aura_node *node, struct aura_buffer *buf, void *arg)
{
	struct lua_bindingsdata *bdata = arg; 
	lua_State *L = bdata->L;
	int nargs = 0; 
	const struct aura_object *o = aura_get_current_object(node);
	const char *fmt = o->ret_fmt;

	lua_rawgeti(L, LUA_REGISTRYINDEX, bdata->inbound_event_ref);
	lua_rawgeti(L, LUA_REGISTRYINDEX, bdata->node_container);
	lua_pushnumber(L, o->id);
	lua_rawgeti(L, LUA_REGISTRYINDEX, bdata->inbound_event_arg_ref);

	while (*fmt) { 
		double tmp;
		switch (*fmt++) { 
		case URPC_U8:
			tmp = aura_buffer_get_u8(buf);
			lua_pushnumber(L, tmp);
			break;
		case URPC_S8:
			tmp = aura_buffer_get_s8(buf);
			lua_pushnumber(L, tmp);
			break;
		case URPC_U16:
			tmp = aura_buffer_get_u16(buf);
			lua_pushnumber(L, tmp);
			break;
		case URPC_S16:
			tmp = aura_buffer_get_s16(buf);
			lua_pushnumber(L, tmp);
			break;
		case URPC_U32:
			tmp = aura_buffer_get_u32(buf);
			lua_pushnumber(L, tmp);
			break;
		case URPC_S32:
			tmp = aura_buffer_get_s32(buf);
			lua_pushnumber(L, tmp);
			break;
		case URPC_U64:
			tmp = aura_buffer_get_u64(buf);
			lua_pushnumber(L, tmp);
			break;
		case URPC_S64:
			tmp = aura_buffer_get_s64(buf);
			lua_pushnumber(L, tmp);
			break;
		case URPC_BIN:
		{
			void *udata;
			const void *srcdata; 
			int len = atoi(fmt);

			if (len == 0) 
				BUG(node, "Internal deserilizer bug processing: %s", fmt);
			udata = lua_newuserdata(L, len);
			if (!udata)
				BUG(node, "Failed to allocate userdata");
			srcdata = aura_buffer_get_bin(buf, len);
			memcpy(udata, srcdata, len);
			break;
			while (*fmt && (*fmt++ != '.'));
		}
		default:
			BUG(node, "Unexpected format token: %s", --fmt);
		}
		nargs++;
	};

	lua_call(L, 3 + nargs, 0);
}


static void calldone_cb(struct aura_node *node, int status, struct aura_buffer *retbuf, void *arg)
{
	struct lua_bindingsdata *bdata = arg;
	lua_State *L = bdata->L;

	TRACE();
	lua_rawgeti(L, LUA_REGISTRYINDEX, bdata->node_container);
	lua_setfield_int(L, "__callresult", status);
	lua_pop(L, 1);
	event_cb(node,retbuf, arg);
}

static int l_start_call(lua_State *L)
{
	struct aura_node *node; 
	struct lua_bindingsdata *bdata; 
	int id, ret, i;
	struct aura_object *o;
	struct aura_buffer *buf;
	const char *fmt;

	TRACE();

	if (!lua_islightuserdata(L, 1)) {
		aura_typeerror(L, 1, "ludata");
	}

	node = lua_touserdata(L, 1);
	bdata = aura_get_userdata(node);

	if (lua_isnumber(L, 2)) { 
		o = aura_etable_find_id(node->tbl, lua_tonumber(L, 2));
	} else if (lua_isstring(L, 2)) { 
		o = aura_etable_find(node->tbl, lua_tostring(L, 2));
	}

	if (!o)
		return luaL_error(L, "Failed to look up object by id or name");

	fmt = o->arg_fmt;

	if (lua_gettop(L) - 2 != o->num_args)
		return luaL_error(L, "Invalid argument count for ", o->name);


	buf = aura_buffer_request(node, o->arglen);
	if (!buf)
		return luaL_error(L, "Epic fail during buffer allocation");

	/* Let's serialize the data, arguments are on the stack, 
	 * Starting from #3.
	 */

	for (i=3; i<=lua_gettop(L); i++) { 
		double tmp; 
		switch (*fmt++) { 
		case URPC_U8:
			tmp = lua_tonumber(L, i);
			aura_buffer_put_u8(buf, tmp);
			break;
		case URPC_S8:
			tmp = lua_tonumber(L, i);
			aura_buffer_put_s8(buf, tmp);
			break;
		case URPC_U16:
			tmp = lua_tonumber(L, i);
			aura_buffer_put_u16(buf, (uint16_t) tmp);
			break;
		case URPC_S16:
			tmp = lua_tonumber(L, i);
			aura_buffer_put_s16(buf, tmp);
			break;
		case URPC_U32:
			tmp = lua_tonumber(L, i);
			aura_buffer_put_u32(buf, tmp);
			break;
		case URPC_S32:
			tmp = lua_tonumber(L, i);
			aura_buffer_put_s32(buf, tmp);
			break;
		case URPC_S64:
			tmp = lua_tonumber(L, i);
			aura_buffer_put_s64(buf, tmp);
			break;
		case URPC_U64:
			tmp = lua_tonumber(L, i);
			aura_buffer_put_u64(buf, tmp);
			break;
			
			/* Binary is the tricky part. String or usata? */	
		case URPC_BIN:
		{
			const char *srcbuf; 
			int len = 0; 
			int blen;
			if (lua_isstring(L, i)) { 
				srcbuf = lua_tostring(L, i);
				len = strlen(srcbuf);
			} else if (lua_isuserdata(L, i)) {
				srcbuf = lua_touserdata(L, i);
			}

			blen = atoi(fmt);

			if (blen == 0) 
				return luaL_error(L, "Internal serilizer bug processing: %s", fmt);

			if (!srcbuf) 
				return luaL_error(L, "Internal bug fetching src pointer");

			if (blen < len)
				len = blen;

			aura_buffer_put_bin(buf, srcbuf, len);
			
			while (*fmt && (*fmt++ != '.'));

			break;
		}
		default: 
			BUG(node, "Unknown token: %s\n", --fmt);
			break;
		}
	}
	
	ret = aura_core_start_call(node, o, calldone_cb, bdata, buf);
	if (ret != 0) {
		/* Don't leak, damn it ! */
		aura_buffer_release(node, buf);
		return luaL_error(L, "Epic fail queueing the call");
	}
	return 0;
}

static int l_set_event_cb(lua_State *L)
{
	struct aura_node *node; 
	struct lua_bindingsdata *bdata; 

	TRACE();
	aura_check_args(L, 2);

	if (!lua_islightuserdata(L, 1)) {
		aura_typeerror(L, 1, "ludata");
	}

	if (!lua_isfunction(L, 2)) {
		aura_typeerror(L, 2, "function");
	}

	node = lua_touserdata(L, 1);
	bdata = aura_get_userdata(node);

	bdata->refs |= REF_EVENT_CB;
	bdata->inbound_event_arg_ref      = luaL_ref(L, LUA_REGISTRYINDEX);
	bdata->inbound_event_ref          = luaL_ref(L, LUA_REGISTRYINDEX);

	aura_unhandled_evt_cb(node, event_cb, bdata);
	
	return 0;
}

static int l_set_status_change_cb(lua_State *L)
{
	struct aura_node *node; 
	struct lua_bindingsdata *bdata; 

	TRACE();
	aura_check_args(L, 2);

	if (!lua_islightuserdata(L, 1)) {
		aura_typeerror(L, 1, "ludata");
	}

	if (!lua_isfunction(L, 2)) {
		aura_typeerror(L, 2, "function");
	}

	node = lua_touserdata(L, 1);
	bdata = aura_get_userdata(node);

	bdata->refs |= REF_STATUS_CB;
	bdata->status_changed_arg_ref = luaL_ref(L, LUA_REGISTRYINDEX);
	bdata->status_changed_ref     = luaL_ref(L, LUA_REGISTRYINDEX);
	aura_status_changed_cb(node, status_cb, bdata);

	/* Synthesize the first status change callback */
	status_cb(node, node->status, bdata);
	return 0;
}

static int l_set_etable_change_cb(lua_State *L)
{
	struct aura_node *node; 
	struct lua_bindingsdata *bdata;

	TRACE();
	aura_check_args(L, 1);
	if (!lua_islightuserdata(L, 1)) {
		aura_typeerror(L, 1, "ludata");
	}
	if (!lua_isfunction(L, 2)) {
		aura_typeerror(L, 2, "ludata");
	}
	node = lua_touserdata(L, 1);
	bdata = aura_get_userdata(node); 
	bdata->refs |= REF_ETABLE_CB;
	bdata->etable_changed_arg_ref = luaL_ref(L, LUA_REGISTRYINDEX);
	bdata->etable_changed_ref     = luaL_ref(L, LUA_REGISTRYINDEX);
	aura_etable_changed_cb(node, etable_cb, bdata);
	/* Synthesize the first status change event */
	etable_cb(node, NULL, node->tbl, bdata);
	return 0;
}


static int l_open_susb(lua_State *L)
{
	const char* cname;
	struct aura_node *node; 

	TRACE();
	aura_check_args(L, 1);
	cname = lua_tostring(L, 1); 
	node = aura_open("simpleusb", cname);
	return check_node_and_push(L, node);
}

static int l_open_dummy(lua_State *L)
{
	struct aura_node *node; 

	TRACE();
	aura_check_args(L, 0);
	node = aura_open("dummy");
	return check_node_and_push(L, node);
}

static int l_open_gpio(lua_State *L)
{
	struct aura_node *node; 

	TRACE();
	aura_check_args(L, 0);
	node = aura_open("gpio");
	return check_node_and_push(L, node);
}

static int l_open_usb(lua_State *L)
{
	const char *vendor, *product, *serial;
	int vid, pid; 
	struct aura_node *node;

	TRACE();
	if (lua_gettop(L) < 2)
		return luaL_error(L, "usb needs at least vid and pid to open");
	vid   = lua_tonumber(L, 1);
	pid   = lua_tonumber(L, 2);

	vendor  = lua_tostring(L, 3);
	product = lua_tostring(L, 4);
	serial  = lua_tostring(L, 5);

	node = aura_open("usb", vid, pid, vendor, product, serial);
	return check_node_and_push(L, node);
}



static int l_set_node_container(lua_State *L)
{
	struct aura_node *node;
	struct lua_bindingsdata *bdata; 

	TRACE();
	aura_check_args(L, 2);
	if (!lua_islightuserdata(L, 1)) {
		aura_typeerror(L, 1, "ludata");
	}
	
	if (!lua_istable(L, 2)) {
		aura_typeerror(L, 2, "table");
	}
	
	node = lua_touserdata(L, 1);
	bdata = aura_get_userdata(node);
	bdata->node_container = luaL_ref(L, LUA_REGISTRYINDEX);
	bdata->refs |= REF_NODE_CONTAINER;
	return 0;
}


static int l_node_status(lua_State *L)
{
	struct aura_node *node;
	struct lua_bindingsdata *bdata; 

	TRACE();
	aura_check_args(L, 1);
	if (!lua_islightuserdata(L, 1)) {
		aura_typeerror(L, 1, "ludata");
	}
	node = lua_touserdata(L, 1);
	
	lua_pushnumber(L, node->status);
	return 1;
}
