#define LUA_LIB

#include <aura/aura.h>
#include <aura/private.h>
#include <search.h>
#include <lua.h>
#include <lauxlib.h>

struct lua_bindingsdata { 
	lua_State *L;
	int status_changed_ref;
	int status_changed_arg_ref;
	int etable_changed_ref;
	int etable_changed_arg_ref;
};

int aura_typeerror (lua_State *L, int narg, const char *tname) 
{
	const char *msg = lua_pushfstring(L, "%s expected, got %s",
					  tname, luaL_typename(L, narg));
	return luaL_argerror(L, narg, msg);
}

static void aura_do_check_args (lua_State *L, const char *func, int need) 
{
	int got = lua_gettop(L);
	if (got < need) { 
		luaL_error(L, "%s expects %d args, %d given",
				  func, need, got);
	}
}

#define aura_check_args(L, need) aura_do_check_args(L, __FUNCTION__, need)

static int l_etable_create (lua_State *L) 
{
	struct aura_node *node;
	int count = 0;
	struct aura_export_table *tbl; 

	aura_check_args(L, 2);
	if (!lua_islightuserdata(L, 1)) {
		aura_typeerror(L, 1, "ludata");
	}
	if (!lua_isnumber(L, 2)) {
		aura_typeerror(L, 1, "number");
	}

	node  = lua_touserdata(L, 1);
	count = lua_tonumber(L, 2); 
	tbl = aura_etable_create(node, count); 
	if (!tbl)
		return luaL_error(L, "error creating etable for %d elements", count);

	lua_pushlightuserdata(L, tbl);
	return 1;
}

static int l_etable_add (lua_State *L) 
{
	struct aura_export_table *tbl; 
	const char *name, *arg, *ret; 
 
	aura_check_args(L, 4);

	if (!lua_islightuserdata(L, 1)) {
		aura_typeerror(L, 1, "ludata");
	}
	if (!lua_isstring(L, 2)) {
		aura_typeerror(L, 1, "string");
	}
	if (!lua_isstring(L, 3)) {
		aura_typeerror(L, 1, "string");
	}
	if (!lua_isstring(L, 4)) {
		aura_typeerror(L, 1, "string");
	}

	tbl  = lua_touserdata(L, 1);
	name = lua_tostring(L,  2);
	arg  = lua_tostring(L,  3);
	ret  = lua_tostring(L,  4);	

	aura_etable_add(tbl, name, arg, ret);	
	return 0;
}

static int l_etable_activate(lua_State *L)
{
	struct aura_export_table *tbl; 
	aura_check_args(L, 1);

	if (!lua_islightuserdata(L, 1)) {
		aura_typeerror(L, 1, "ludata");
	}

	tbl = lua_touserdata(L, 1);
	aura_etable_activate(tbl);
	return 0;
}

static inline int check_node_and_push(lua_State *L, struct aura_node *node) 
{
	if (node) {
		lua_pushlightuserdata(L, node);
		struct lua_bindingsdata *bdata = calloc(1, sizeof(*bdata));
		if (!bdata)
			return luaL_error(L, "Memory allocation error");
		bdata->L = L;
		aura_set_userdata(node, bdata);

	} else {  
		lua_pushnil(L);
	}
	return 1;
}

static int l_close(lua_State *L)
{
	struct aura_node *node; 
	struct lua_bindingsdata *bdata;
 	
	aura_check_args(L, 1);
	if (!lua_islightuserdata(L, 1)) {
		aura_typeerror(L, 1, "ludata");
	}
	node = lua_touserdata(L, 1);
	bdata = aura_get_userdata(node);
	/* We assume that we've set all the callback on open 
	 * in lua counterpart 
	 */
	luaL_unref(L, LUA_REGISTRYINDEX, bdata->status_changed_ref);
	luaL_unref(L, LUA_REGISTRYINDEX, bdata->status_changed_arg_ref);
	luaL_unref(L, LUA_REGISTRYINDEX, bdata->etable_changed_ref);
	luaL_unref(L, LUA_REGISTRYINDEX, bdata->etable_changed_arg_ref);
	free(bdata);
	aura_close(node);
	return 0;
}

static int l_handle_events(lua_State *L)
{
	struct aura_node *node; 
	struct aura_eventloop *loop; 

	aura_check_args(L, 1);
	if (!lua_islightuserdata(L, 1)) {
		aura_typeerror(L, 1, "ludata");
	}

	node = lua_touserdata(L, 1);
	loop = aura_eventloop_get_data(node);
	if (!loop) 
		luaL_error(L, "BUG: No eventsystem in node"); 

	aura_handle_events(loop);
	return 0;
}

static void status_cb(struct aura_node *node, int newstatus, void *arg)
{
	struct lua_bindingsdata *bdata = arg; 
	lua_State *L = bdata->L;
	lua_rawgeti(L, LUA_REGISTRYINDEX, bdata->status_changed_ref);
	lua_rawgeti(L, LUA_REGISTRYINDEX, bdata->status_changed_arg_ref);
	lua_pushnumber(L, newstatus);
	lua_call(L, 2, 0);
}

static void etable_cb(struct aura_node *node, void *arg)
{
	struct lua_bindingsdata *bdata = arg; 
	lua_State *L = bdata->L;
	lua_rawgeti(L, LUA_REGISTRYINDEX, bdata->etable_changed_ref);
	lua_rawgeti(L, LUA_REGISTRYINDEX, bdata->etable_changed_arg_ref);
	lua_call(L, 1, 0);
}

static int l_set_status_change_cb(lua_State *L)
{
	struct aura_node *node; 
	struct lua_bindingsdata *bdata; 
	aura_check_args(L, 2);
	if (!lua_islightuserdata(L, 1)) {
		aura_typeerror(L, 1, "ludata");
	}
	if (!lua_isfunction(L, 2)) {
		aura_typeerror(L, 2, "ludata");
	}
	node = lua_touserdata(L, 1);
	bdata = aura_get_userdata(node); 
	bdata->status_changed_arg_ref = luaL_ref(L, LUA_REGISTRYINDEX);
	bdata->status_changed_ref     = luaL_ref(L, LUA_REGISTRYINDEX);
	aura_status_changed_cb(node, status_cb, bdata);
	return 0;
}

static int l_set_etable_change_cb(lua_State *L)
{
	struct aura_node *node; 
	struct lua_bindingsdata *bdata;
	aura_check_args(L, 1);
	if (!lua_islightuserdata(L, 1)) {
		aura_typeerror(L, 1, "ludata");
	}
	if (!lua_isfunction(L, 2)) {
		aura_typeerror(L, 2, "ludata");
	}
	node = lua_touserdata(L, 1);
	bdata = aura_get_userdata(node); 
	bdata->etable_changed_arg_ref = luaL_ref(L, LUA_REGISTRYINDEX);
	bdata->etable_changed_ref     = luaL_ref(L, LUA_REGISTRYINDEX);
	aura_etable_changed_cb(node, etable_cb, bdata);
	return 0;
}

static void lua_setfield_string(lua_State *L, const char *key, const char *value)
{
	lua_pushstring(L, key);
	lua_pushstring(L, value);
	lua_settable(L, -3);
} 

static void lua_setfield_int(lua_State *L, const char *key, long value)
{
	lua_pushstring(L, key);
	lua_pushnumber(L, value);
	lua_settable(L, -3);
} 

static void lua_setfield_bool(lua_State *L, const char *key, bool value)
{
	lua_pushstring(L, key);
	lua_pushboolean(L, value);
	lua_settable(L, -3);
} 

static int l_get_exports(lua_State *L)
{
	struct aura_node *node; 
	struct aura_export_table *tbl; 
	int i;
	aura_check_args(L, 1);
	if (!lua_islightuserdata(L, 1)) {
		aura_typeerror(L, 1, "ludata");
	}
	node = lua_touserdata(L, 1);
	tbl = node->tbl;
	lua_newtable(L);
	for (i=0; i<tbl->next; i++) { 
		struct aura_object *o = &tbl->objects[i];
		lua_pushinteger(L, i);
		lua_newtable(L);

		lua_setfield_string(L, "name",  o->name);
		lua_setfield_bool(L,   "valid", o->valid);
		lua_setfield_int(L,    "id",    o->id);
		if (o->arg_fmt)
			lua_setfield_string(L, "arg", o->arg_fmt);
		if (o->ret_fmt)
			lua_setfield_string(L, "ret", o->ret_fmt);
		if (o->arg_pprinted)
			lua_setfield_string(L, "arg_pprint", o->arg_pprinted);
		if (o->arg_pprinted)
			lua_setfield_string(L, "ret_pprint", o->arg_pprinted);
		
		lua_settable(L, -3);
	}
	return 1;
}

static int l_open_susb(lua_State *L)
{
	const char* cname;
	struct aura_node *node; 
	aura_check_args(L, 1);
	cname = lua_tostring(L, 1); 
	node = aura_open("simpleusb", cname);
	return check_node_and_push(L, node);
}

static int l_open_dummy(lua_State *L)
{
	struct aura_node *node; 
	aura_check_args(L, 0);
	node = aura_open("dummy");
	return check_node_and_push(L, node);
}

static int l_open_usb(lua_State *L)
{
	const char *vendor, *product, *serial;
	int vid, pid; 
	struct aura_node *node;

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

static int l_slog_init(lua_State *L)
{
	const char *fname;
	int level; 

	aura_check_args(L, 2);
	fname = lua_tostring(L, 1); 
	level = lua_tonumber(L, 2); 
	slog_init(fname, level);
	return 0;
}

static int l_handle_events_timeout(lua_State *L)
{
	
}

static const luaL_Reg openfuncs[] = {
	{ "dummy",     l_open_dummy},
	{ "simpleusb", l_open_susb },
	{ "usb",       l_open_usb },
	{NULL, NULL}
};

static const luaL_Reg libfuncs[] = {
	{ "handle_events",   l_handle_events               },
	{ "status_cb",       l_set_status_change_cb        },
	{ "etable_cb",       l_set_etable_change_cb        },
	{ "etable_create",   l_etable_create    },
	{ "etable_get",      l_get_exports      },
	{ "etable_add",      l_etable_add       },
	{ "etable_activate", l_etable_activate  },
	{ "slog_init",       l_slog_init        },
	{ "close",           l_close            },
	{NULL, NULL}
};


LUALIB_API int luaopen_auracore (lua_State *L) 
{
	printf("A.U.R.A. Extension loaded.\n");
	luaL_newlib(L, libfuncs);

	/* 
	 * Push open functions as aura["openfunc"]
	 */

	lua_pushstring(L, "openfuncs");
	lua_newtable(L);
	luaL_setfuncs(L, openfuncs, 0);	
	lua_settable(L, -3);

	/* Return One result */
	return 1;
}
