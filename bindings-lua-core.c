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

#define aura_check_args(L, need) aura_do_check_args(L, __FUNCTION__, need)

#define laura_eventloop_type "laura_eventloop"
#define laura_node_type      "laura_node"

struct laura_node { 
	lua_State *L;
	struct aura_node *node;
	uint32_t refs;
	int node_container; /* lua table representing this node */
	int status_changed_ref;
	int status_changed_arg_ref;
	int etable_changed_ref;
	int etable_changed_arg_ref;
	int inbound_event_ref;
	int inbound_event_arg_ref;
};


static int l_node_gc(lua_State *L)
{
	slog(4, SLOG_DEBUG, "Lua GC on node object");
	return 0;
}

static const struct luaL_Reg node_meta[] = {
  {"__gc",   l_node_gc      },
  {NULL,     NULL           }
};

static inline int check_node_and_push(lua_State *L, struct aura_node *node) 
{
	if (node) {
		struct laura_node *bdata = lua_newuserdata(L, sizeof(void *));
		if (!bdata)
			return luaL_error(L, "Memory allocation error");
		bzero(bdata, sizeof(*bdata));
		bdata->L = L;
		bdata->node = node; 
		aura_set_userdata(node, bdata);
		luaL_setmetatable(L, laura_node_type);
	} else {  
		lua_pushnil(L);
	}
	return 1;
}


#define PREPARE_ARG(n)							\
	const void *arg ## n ##_ptr;					\
	const int arg ## n ##_int;					\
	if (lua_gettop(L) > (1 + n)) {					\
		if (lua_isstring(L, (1 + n)))				\
			arg ## n ## _ptr = lua_tostring(L, (1 + n));	\
		else if (lua_isnumber(L, (1 + n)))			\
			arg ## n ## _int = lua_tonumber(L, (1 + n));	\
	}								\
									\

#define ARG(n) (lua_isstring(L, 1 + n) ? arg ## n ## _ptr : arg ## n ## _int)

static int l_open_node(lua_State *L)
{

	int n;
	struct aura_node *node;
	TRACE();
	if (lua_gettop(L) < 1)
		return luaL_error(L, "Open needs at least 1 argument");

	node = aura_open(lua_tostring(L, 1), lua_tostring(L, 2));
	return check_node_and_push(L, node);
}

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

static int lua_push_etable(lua_State *L, struct aura_export_table *tbl)
{
	int i;
	if (!tbl) { 
		lua_pushnil(L);
		return 1;
	}

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


static int l_etable_get(lua_State *L)
{
	struct aura_node *node; 

	TRACE();
	aura_check_args(L, 1);
	if (!lua_islightuserdata(L, 1)) {
		aura_typeerror(L, 1, "ludata");
	}
	node = lua_touserdata(L, 1);
	return lua_push_etable(L, node->tbl);
}


static int l_etable_create (lua_State *L) 
{
	struct aura_node *node;
	int count = 0;	
	struct aura_export_table *tbl; 
	
	TRACE();
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
 
	TRACE();
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
	
	TRACE();
	aura_check_args(L, 1);

	if (!lua_islightuserdata(L, 1)) {
		aura_typeerror(L, 1, "ludata");
	}

	tbl = lua_touserdata(L, 1);
	aura_etable_activate(tbl);
	return 0;
}


/* --------------------------------------- */


/*
static int l_eventloop_create(lua_State *L)
{
	struct aura_node *node; 
	struct aura_eventloop *loop; 

	TRACE();
	aura_check_args(L, 1);
	if (!lua_islightuserdata(L, 1)) {
		aura_typeerror(L, 1, "ludata");
	}
	node = lua_touserdata(L, 1);
	loop = aura_eventloop_create(node);
	if (!loop) 
		lua_pushnil(L);
	else
		lua_pushlightuserdata(L, loop);
	return 1;
}

static int l_eventloop_add(lua_State *L)
{
	struct aura_node *node; 
	struct aura_eventloop *loop; 

	TRACE();
	aura_check_args(L, 2);
	if (!lua_islightuserdata(L, 1)) {
		aura_typeerror(L, 1, "ludata (loop)");
	}
	if (!lua_islightuserdata(L, 2)) {
		aura_typeerror(L, 2, "ludata (node)");
	}

	loop = lua_touserdata(L, 1);
	node = lua_touserdata(L, 2);
	
	aura_eventloop_add(loop, node);
	return 0;
}

static int l_eventloop_del(lua_State *L)
{
	struct aura_node *node; 

	TRACE();
	aura_check_args(L, 1);

	if (!lua_islightuserdata(L, 1)) {
		aura_typeerror(L, 1, "ludata (node)");
	}

	node = lua_touserdata(L, 2);
	
	aura_eventloop_del(node);
	return 0;
}

static int l_eventloop_destroy(lua_State *L)
{
	struct aura_eventloop *loop; 

	TRACE();
	aura_check_args(L, 1);

	if (!lua_islightuserdata(L, 1)) {
		aura_typeerror(L, 1, "ludata (loop)");
	}

	loop = lua_touserdata(L, 1);
	aura_eventloop_destroy(loop);

	return 0;
}


static const struct luaL_Reg evtloop_meta[] = {
  {"__gc",   l_eventloop_gc},
  {"__call", l_eventloop_create},
  {NULL, NULL}
};

*/


static int l_slog_init(lua_State *L)
{
	const char *fname;
	int level; 

	TRACE();
	aura_check_args(L, 2);
	fname = lua_tostring(L, 1); 
	level = lua_tonumber(L, 2); 
	slog_init(fname, level);
	return 0;
}


static const luaL_Reg libfuncs[] = {
	{ "slog_init",                 l_slog_init                   },	
	{ "etable_create",             l_etable_create               },
	{ "etable_get",                l_etable_get                  },
	{ "etable_add",                l_etable_add                  },
	{ "etable_activate",           l_etable_activate             },
	{ "open_node",                 l_open_node                   },


/*
	{ "set_node_containing_table", l_set_node_container          }, 

	{ "status_cb",                 l_set_status_change_cb        },
	{ "etable_cb",                 l_set_etable_change_cb        },
	{ "event_cb",                  l_set_event_cb                },
	{ "core_close",                l_close                       },

	{ "handle_events",             l_handle_events               },
	{ "eventloop_create",          l_eventloop_create            },
	{ "eventloop_add",             l_eventloop_add               },
	{ "eventloop_del",             l_eventloop_del               },
	{ "eventloop_destroy",         l_eventloop_destroy           },
	
	{ "start_call",                l_start_call                  },
	{ "node_status",               l_node_status                 },
*/
	{NULL,                         NULL}
};


LUALIB_API int luaopen_auracore (lua_State *L) 
{
	luaL_newmetatable(L, laura_node_type);
	luaL_setfuncs(L, node_meta, 0);

	luaL_newlib(L, libfuncs);


	/* 
	 * Push open functions as aura["openfunc"]
	 */

	/*
	lua_pushstring(L, "openfuncs");
	lua_newtable(L);
	luaL_setfuncs(L, openfuncs, 0);	
	lua_settable(L, -3);
	*/

	/* Return One result */
	return 1;
}
