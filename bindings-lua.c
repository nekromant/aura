#define LUA_LIB

#include <aura/aura.h>
#include <aura/private.h>
#include <search.h>
#include <lua.h>
#include <lauxlib.h>

#define REF_NODE_CONTAINER (1 << 0)
#define REF_STATUS_CB      (1 << 1)
#define REF_ETABLE_CB      (1 << 2)
#define REF_EVENT_CB       (1 << 3)

//#define TRACE_BCALLS

#ifdef TRACE_BCALLS
#define TRACE() slog(4, SLOG_DEBUG, "Bindings call: %s", __func__)
#else
#define TRACE()
#endif

#define aura_check_args(L, need) aura_do_check_args(L, __FUNCTION__, need)

#define laura_eventloop_type "laura_eventloop"
#define laura_node_type      "laura_node"

extern int lua_stackdump(lua_State *L);


#if LUA_VERSION_NUM < 502
#warning Building with lua-5.1-compat hacks


#define luaL_setmetatable(L, name) \
	luaL_getmetatable(L, name); \
	lua_setmetatable(L, -2)

#define setfuncs(L, l) luaL_setfuncs(L, l, 0)

static void luaL_setfuncs(lua_State *L, const luaL_Reg *l, int nup)
{
	luaL_checkstack(L, nup + 1, "too many upvalues");
	for (; l->name != NULL; l++) { /* fill the table with given functions */
		int i;
		lua_pushstring(L, l->name);
		for (i = 0; i < nup; i++)               /* copy upvalues to the top */
			lua_pushvalue(L, -(nup + 1));
		lua_pushcclosure(L, l->func, nup);      /* closure with those upvalues */
		lua_settable(L, -(nup + 3));
	}
	lua_pop(L, nup); /* remove upvalues */
}

#define luaL_newlib(L, l) \
	(lua_newtable((L)), luaL_setfuncs((L), (l), 0))

#endif


/* Lightuserdata can't have an attached metatable so we have to resort to
 * using full userdata here.
 * Bindings take care to close nodes and destroy eventloops when
 * garbage-collecting
 */
struct laura_node {
	lua_State *		L;
	struct aura_node *	node;
	const char *		current_call;
	uint32_t		refs;
	int			node_container; /* lua table representing this node */
	int			status_changed_ref;
	int			status_changed_arg_ref;
	int			inbound_event_ref;
	int			inbound_event_arg_ref;
};

struct laura_eventloop {
	struct aura_eventloop *loop;
};

static inline int check_node_and_push(lua_State *L, struct aura_node *node)
{
	if (node) {
		struct laura_node *bdata = lua_newuserdata(L, sizeof(*bdata));
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


#define PREPARE_ARG(n)                                                  \
	const void *arg ## n ## _ptr;                                    \
	const int arg ## n ## _int;                                      \
	if (lua_gettop(L) > (1 + n)) {                                  \
		if (lua_isstring(L, (1 + n)))                           \
			arg ## n ## _ptr = lua_tostring(L, (1 + n));    \
		else if (lua_isnumber(L, (1 + n)))                      \
			arg ## n ## _int = lua_tonumber(L, (1 + n));    \
	}                                                               \
                                                                        \

#define ARG(n) (lua_isstring(L, 1 + n) ? arg ## n ## _ptr : arg ## n ## _int)


static int aura_typeerror(lua_State *L, int narg, const char *tname)
{
	const char *msg = lua_pushfstring(L, "%s expected, got %s",
					  tname, luaL_typename(L, narg));

	return luaL_argerror(L, narg, msg);
}

static struct laura_node *lua_fetch_node(lua_State *L, int idx)
{
	struct laura_node *lnode = NULL;

	if (lua_isuserdata(L, idx)) {
		lnode = lua_touserdata(L, idx);
	} else if (lua_istable(L, idx)) {
		lua_pushstring(L, "__node");
		lua_gettable(L, idx);
		lnode = lua_touserdata(L, -1);
	}
	lua_pop(L, 1);
	return lnode;
}

static void aura_do_check_args(lua_State *L, const char *func, int need)
{
	int got = lua_gettop(L);

	if (got < need)
		luaL_error(L, "%s expects %d args, %d given",
			   func, need, got);
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
	for (i = 0; i < tbl->next; i++) {
		struct aura_object *o = &tbl->objects[i];
		lua_pushinteger(L, i);
		lua_newtable(L);

		lua_setfield_string(L, "name", o->name);
		lua_setfield_bool(L, "valid", o->valid);
		lua_setfield_int(L, "id", o->id);
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

static int buffer_to_lua(lua_State *L, struct aura_node *node, const struct aura_object *o, struct aura_buffer *buf)
{
	const char *fmt = o->ret_fmt;
	int nargs = 0;

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
			while (*fmt && (*fmt++ != '.'));
			break;
		}
		default:
			BUG(node, "Unexpected format token: %s", --fmt);
		}
		nargs++;
	}
	;

	return nargs;
}

static struct aura_buffer *lua_to_buffer(lua_State *L, struct aura_node *node, int stackpos, struct aura_object *o)
{
	int i;
	struct aura_buffer *buf;
	const char *fmt;

	fmt = o->arg_fmt;
	if (lua_gettop(L) - stackpos + 1 != o->num_args) {
		slog(0, SLOG_ERROR, "Invalid argument count for %s: %d / %d",
		     o->name, lua_gettop(L) - stackpos, o->num_args);
		return NULL;
	}

	buf = aura_buffer_request(node, o->arglen);
	if (!buf) {
		slog(0, SLOG_ERROR, "Epic fail during buffer allocation");
		return NULL;
	}

	/* Let's serialize the data, arguments are on the stack,
	 * Starting from #3.
	 */

	for (i = stackpos; i <= lua_gettop(L); i++) {
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
			aura_buffer_put_u16(buf, (uint16_t)tmp);
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
			const char *srcbuf = NULL;
			int len = 0;
			int blen;
			if (lua_isstring(L, i)) {
				srcbuf = lua_tostring(L, i);
				len = strlen(srcbuf);
			} else if (lua_isuserdata(L, i)) {
				srcbuf = lua_touserdata(L, i);
			}

			blen = atoi(fmt);

			if (blen == 0) {
				slog(0, SLOG_ERROR, "Internal serilizer bug processing: %s", fmt);
				goto err;
			}

			if (!srcbuf) {
				slog(0, SLOG_ERROR, "Internal bug fetching src pointer");
				goto err;
			}

			if (blen < len)
				len = blen;

			aura_buffer_put_bin(buf, srcbuf, len);

			while (*fmt && (*fmt++ != '.'));

			break;
		}
		default:
			BUG(node, "Unknown token: %c\n", *(--fmt));
			break;
		}
	}

	return buf;
err:
	aura_buffer_release(buf);
	return NULL;
}

static int l_open_node(lua_State *L)
{
	struct aura_node *node;

	TRACE();
	aura_check_args(L, 2);
	node = aura_open(lua_tostring(L, 1), lua_tostring(L, 2));
	if (!node)
		return luaL_error(L, "Failed to open node");

	return check_node_and_push(L, node);
}

static int l_close_node(lua_State *L)
{
	struct laura_node *lnode = lua_fetch_node(L, -1);
	struct aura_node *node = lnode->node;

	TRACE();

	/* Handle weird cases when we've already cleaned up */
	if (!node)
		return 0;

	/* Clear up references we've set up so far*/
	if (lnode->refs & REF_NODE_CONTAINER)
		luaL_unref(L, LUA_REGISTRYINDEX, lnode->node_container);

	if (lnode->refs & REF_STATUS_CB) {
		luaL_unref(L, LUA_REGISTRYINDEX, lnode->status_changed_ref);
		luaL_unref(L, LUA_REGISTRYINDEX, lnode->status_changed_arg_ref);
	}

/*
 *      if (lnode->refs & REF_ETABLE_CB) {
 *              luaL_unref(L, LUA_REGISTRYINDEX, lnode->etable_changed_ref);
 *              luaL_unref(L, LUA_REGISTRYINDEX, lnode->etable_changed_arg_ref);
 *      }
 */

	aura_close(node);
	lnode->node = NULL;
	return 0;
}

static int l_node_gc(lua_State *L)
{
	return l_close_node(L);
}

static int laura_do_sync_call(lua_State *L)
{
	struct laura_node *lnode = lua_fetch_node(L, 1);
	struct aura_buffer *buf, *retbuf;
	struct aura_object *o;
	int ret;

	TRACE();

	o = aura_etable_find(lnode->node->tbl, lnode->current_call);
	if (!o)
		return luaL_error(L, "Attempt to call non-existend method");

	buf = lua_to_buffer(L, lnode->node, 2, o);
	if (!buf)
		return luaL_error(L, "Serializer failed!");

	ret = aura_core_call(lnode->node, o, &retbuf, buf);
	if (ret != 0)
		return luaL_error(L, "Call for %s failed", o->name);

	ret = buffer_to_lua(L, lnode->node, o, retbuf);
	aura_buffer_release(retbuf);
	return ret;
}


static void calldone_cb(struct aura_node *node, int status, struct aura_buffer *retbuf, void *arg)
{
	struct laura_node *lnode;
	lua_State *L;
	const struct aura_object *o;

	lnode = aura_get_userdata(node);
	L = lnode->L;
	o = aura_get_current_object(lnode->node);
	TRACE();


	lua_rawgeti(L, LUA_REGISTRYINDEX, (long)arg);  /* Fetch our callback params */
	lua_pushnumber(L, 1);
	lua_gettable(L, -2);

	lua_rawgeti(L, LUA_REGISTRYINDEX, lnode->node_container);

	lua_pushnumber(L, status);

	lua_pushnumber(L, 2);
	lua_gettable(L, -5);

	buffer_to_lua(L, lnode->node, o, retbuf);

	lua_stackdump(L);

	lua_call(L, 3 + o->num_rets, 0);                /* Fire the callback! */

	luaL_unref(L, LUA_REGISTRYINDEX, (long)arg);    /* Remove the reference */
}

static int laura_do_async_call(lua_State *L)
{
	struct laura_node *lnode = NULL;
	const char *name;
	struct aura_buffer *buf;
	struct aura_object *o;
	int ret;
	int callback_ref;

	TRACE();

	/* Sanity */
	lnode = lua_fetch_node(L, 1);
	if (!lnode) {
		lua_stackdump(L);
		return aura_typeerror(L, 1, "userdata (node)");
	}

	if (!lua_isstring(L, 2)) {
		lua_stackdump(L);
		return aura_typeerror(L, 2, "string (object name)");
	}

	if (!lua_isfunction(L, 3)) {
		lua_stackdump(L);
		return aura_typeerror(L, 3, "function (callback)");
	}

	name = lua_tostring(L, 2);

	o = aura_etable_find(lnode->node->tbl, name);
	if (!o)
		return luaL_error(L, "Attempt to call non-existend method");

	if (!object_is_method(o)) {
		lua_stackdump(L);
		return luaL_error(L, "Attempt to call an event");
	}


	/* Now we're sane! */

	buf = lua_to_buffer(L, lnode->node, 5, o);
	if (!buf)
		luaL_error(L, "Serializer failed!");

	/* Let's create a table to store our callback and arg */
	lua_newtable(L);

	/* Push the callback function there */
	lua_pushnumber(L, 1);
	lua_pushvalue(L, 3);
	lua_settable(L, -3);

	/* And the user argument */
	lua_pushnumber(L, 2);
	lua_pushvalue(L, 4);
	lua_settable(L, -3);

	/* And fetch the reference to out table that we'll use in callback */
	callback_ref = luaL_ref(L, LUA_REGISTRYINDEX);
	slog(4, SLOG_DEBUG, "Callback tbl reference: %d", callback_ref);

	ret = aura_core_start_call(lnode->node, o, calldone_cb, (void *)(long)callback_ref, buf);
	if (ret != 0) {
		aura_buffer_release(buf);
		return luaL_error(L, "Async call for %s failed: %d:%s", o->name, ret, strerror(ret));
	}

	return 0;
}

static int l_node_index(lua_State *L)
{
	struct laura_node *lnode = lua_touserdata(L, 1);
	const char *name = lua_tostring(L, -1);
	struct aura_object *o;

	TRACE();
	/* FixMe: Can this get gc-d by the time we actually use it? */
	lnode->current_call = name;

	if (AURA_STATUS_ONLINE != aura_get_status(lnode->node))
		return luaL_error(L, "Attempt to call remote method %s when node offline",
			lnode->current_call);
	
	o = aura_etable_find(lnode->node->tbl, lnode->current_call);
	if (!o)
		return luaL_error(L, "Internal aura bug: failed to lookup object: %s",
			lnode->current_call);

	if (strcmp("__", name) == 0)
		lua_pushcfunction(L, laura_do_async_call);
	else if (object_is_method(o))
		lua_pushcfunction(L, laura_do_sync_call);
	else
		lua_pushnil(L);
	return 1;
}



static int l_etable_get(lua_State *L)
{
	struct laura_node *lnode = NULL;
	struct aura_node *node = NULL;

	TRACE();
	aura_check_args(L, 1);

	if (lua_islightuserdata(L, 1))
		node = lua_touserdata(L, 1);
	else
		lnode = lua_fetch_node(L, 1);
	if (lnode)
		node = lnode->node;

	if (!node)
		return luaL_error(L, "Failed to fetch node");

	return lua_push_etable(L, node->tbl);
}


static int l_etable_create(lua_State *L)
{
	struct aura_node *node;
	int count = 0;
	struct aura_export_table *tbl;

	TRACE();
	aura_check_args(L, 2);
	if (!lua_islightuserdata(L, 1))
		aura_typeerror(L, 1, "ludata");
	if (!lua_isnumber(L, 2))
		aura_typeerror(L, 1, "number");

	node = lua_touserdata(L, 1);
	count = lua_tonumber(L, 2);
	tbl = aura_etable_create(node, count);
	if (!tbl)
		return luaL_error(L, "error creating etable for %d elements", count);

	lua_pushlightuserdata(L, tbl);
	return 1;
}

static int l_etable_add(lua_State *L)
{
	struct aura_export_table *tbl;
	const char *name, *arg, *ret;

	TRACE();
	aura_check_args(L, 4);

	if (!lua_islightuserdata(L, 1))
		aura_typeerror(L, 1, "ludata");
	if (!lua_isstring(L, 2))
		aura_typeerror(L, 1, "string");
	if (!lua_isstring(L, 3))
		aura_typeerror(L, 1, "string");
	if (!lua_isstring(L, 4))
		aura_typeerror(L, 1, "string");

	tbl = lua_touserdata(L, 1);
	name = lua_tostring(L, 2);
	arg = lua_tostring(L, 3);
	ret = lua_tostring(L, 4);

	aura_etable_add(tbl, name, arg, ret);
	return 0;
}

static int l_etable_activate(lua_State *L)
{
	struct aura_export_table *tbl;

	TRACE();
	aura_check_args(L, 1);

	if (!lua_islightuserdata(L, 1))
		aura_typeerror(L, 1, "ludata");

	tbl = lua_touserdata(L, 1);
	aura_etable_activate(tbl);
	return 0;
}


static int l_eventloop_create(lua_State *L)
{
	struct laura_node *lnode;
	struct laura_eventloop *lloop;

	TRACE();

	lnode = lua_fetch_node(L, 1);
	if (!lnode)
		return luaL_error(L, "Failed to fetch node");

	lloop = lua_newuserdata(L, sizeof(*lloop));
	if (!lloop)
		return luaL_error(L, "Userdata allocation failed");
	luaL_setmetatable(L, laura_eventloop_type);

	lloop->loop = aura_eventloop_create(lnode->node);
	if (!lloop->loop)
		lua_pushnil(L);

	return 1;
}

static int l_eventloop_add(lua_State *L)
{
	struct laura_node *lnode;
	struct laura_eventloop *lloop;

	TRACE();
	aura_check_args(L, 2);

	if (!lua_isuserdata(L, 1))
		aura_typeerror(L, 1, "ludata (eventloop)");
	lnode = lua_fetch_node(L, 2);

	lloop = lua_touserdata(L, 1);
	if (!lloop || !lnode)
		return luaL_error(L, "Failed to retrive arguments");

	aura_eventloop_add(lloop->loop, lnode->node);

	return 0;
}

static int l_eventloop_del(lua_State *L)
{
	struct laura_node *lnode;

	TRACE();
	aura_check_args(L, 1);

	lnode = lua_fetch_node(L, 1);

	if (!lnode)
		return luaL_error(L, "Failed to retrive arguments");

	aura_eventloop_del(lnode->node);
	return 0;
}

static int l_eventloop_destroy(lua_State *L)
{
	struct laura_eventloop *lloop;

	TRACE();
	aura_check_args(L, 1);

	if (!lua_isuserdata(L, 1))
		aura_typeerror(L, 2, "ludata (eventloop)");

	lloop = lua_touserdata(L, 1);
	if (!lloop)
		return luaL_error(L, "Failed to retrive arguments");

	if (!lloop->loop)
		return 0;

	aura_eventloop_destroy(lloop->loop);
	lloop->loop = NULL;

	return 0;
}

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

static int l_wait_status(lua_State *L)
{
	struct laura_node *lnode;

	TRACE();
	aura_check_args(L, 2);
	lnode = lua_fetch_node(L, 1);
	aura_wait_status(lnode->node, lua_tonumber(L, 2));
	return 0;
}

static void event_cb(struct aura_node *node, struct aura_buffer *buf, void *arg)
{
	struct laura_node *lnode = arg;
	lua_State *L = lnode->L;
	const struct aura_object *o = aura_get_current_object(node);
	int nargs;

	TRACE();

	lua_rawgeti(L, LUA_REGISTRYINDEX, lnode->node_container);
	lua_pushstring(L, o->name);
	lua_gettable(L, -2);

	if (!lua_isfunction(L, -1)) {
		slog(0, SLOG_WARN, "Dropping unhandled event: %s", o->name);
		return;
	}

	lua_rawgeti(L, LUA_REGISTRYINDEX, lnode->node_container);

	nargs = buffer_to_lua(L, lnode->node, o, buf);
	lua_call(L, nargs + 1, 0);
}

static int l_set_node_container(lua_State *L)
{
	struct laura_node *lnode;

	TRACE();
	aura_check_args(L, 2);
	if (!lua_isuserdata(L, 1))
		aura_typeerror(L, 1, "udata");

	if (!lua_istable(L, 2))
		aura_typeerror(L, 2, "table");

	lnode = lua_touserdata(L, 1);
	lnode->node_container = luaL_ref(L, LUA_REGISTRYINDEX);
	lnode->refs |= REF_NODE_CONTAINER;

	aura_unhandled_evt_cb(lnode->node, event_cb, lnode);

	return 0;
}

static int l_status(lua_State *L)
{
	struct laura_node *lnode;

	TRACE();
	aura_check_args(L, 1);
	lnode = lua_fetch_node(L, 1);
	if (!lnode)
		return luaL_error(L, "Failed to retrieve lnode");
	lua_pushnumber(L, aura_get_status(lnode->node));
	return 1;
}


static void status_cb(struct aura_node *node, int newstatus, void *arg)
{
	struct laura_node *lnode = arg;
	lua_State *L = lnode->L;

	lua_rawgeti(L, LUA_REGISTRYINDEX, lnode->status_changed_ref);
	lua_rawgeti(L, LUA_REGISTRYINDEX, lnode->node_container);
	lua_pushnumber(L, newstatus);
	lua_rawgeti(L, LUA_REGISTRYINDEX, lnode->status_changed_arg_ref);
	lua_call(L, 3, 0);
}

static int l_set_status_change_cb(lua_State *L)
{
	struct laura_node *lnode;
	struct aura_node *node;

	TRACE();
	aura_check_args(L, 3);

	lnode = lua_fetch_node(L, 1);
	if (!lnode)
		return luaL_error(L, "Failed to retrieve node");

	node = lnode->node;

	if (!lua_isfunction(L, 2))
		aura_typeerror(L, 2, "function");

	if (lnode->refs & REF_STATUS_CB) {
		luaL_unref(L, LUA_REGISTRYINDEX, lnode->status_changed_arg_ref);
		luaL_unref(L, LUA_REGISTRYINDEX, lnode->status_changed_ref);
	}

	lnode->refs |= REF_STATUS_CB;
	lnode->status_changed_arg_ref = luaL_ref(L, LUA_REGISTRYINDEX);
	lnode->status_changed_ref = luaL_ref(L, LUA_REGISTRYINDEX);
	aura_status_changed_cb(node, status_cb, lnode);

	return 0;
}

static int l_handle_events(lua_State *L)
{
	struct laura_eventloop *lloop;
	int timeout = -1;

	TRACE();
	aura_check_args(L, 1);

	if (!lua_isuserdata(L, 1))
		aura_typeerror(L, 1, "ludata");

	lloop = lua_touserdata(L, 1);

	if (lua_gettop(L) == 2) {
		timeout = lua_tonumber(L, 2);
		aura_handle_events_timeout(lloop->loop, timeout);
	} else {
		aura_handle_events_forever(lloop->loop);
	}

	return 0;
}

static const luaL_Reg libfuncs[] = {
	{ "slog_init",		       l_slog_init	      },
	{ "etable_create",	       l_etable_create	      },
	{ "etable_get",		       l_etable_get	      },
	{ "etable_add",		       l_etable_add	      },
	{ "etable_activate",	       l_etable_activate      },
	{ "core_open",		       l_open_node	      },
	{ "core_close",		       l_close_node	      },
	{ "wait_status",	       l_wait_status	      },

	{ "status",		       l_status		      },
	{ "status_cb",		       l_set_status_change_cb },

	{ "set_node_containing_table", l_set_node_container   },
	{ "eventloop_create",	       l_eventloop_create     },
	{ "eventloop_add",	       l_eventloop_add	      },
	{ "eventloop_del",	       l_eventloop_del	      },
	{ "eventloop_destroy",	       l_eventloop_destroy    },

	{ "handle_events",	       l_handle_events	      },

/*
 *      { "status_cb",                 l_set_status_change_cb        },
 *      { "event_cb",                  l_set_event_cb                },
 *
 */
	{ NULL,			       NULL		      }
};


static const struct luaL_Reg node_meta[] = {
	{ "__gc",    l_node_gc	  },
	{ "__index", l_node_index },
	{ NULL,	     NULL	  }
};


static int l_loop_gc(lua_State *L)
{
	slog(0, SLOG_WARN, "GC on a evtloop: This shouldn't normally happen, but we'll close the node anyway");
	return l_eventloop_destroy(L);
}


static const struct luaL_Reg loop_meta[] = {
	{ "__gc", l_loop_gc },
	{ NULL,	  NULL	    }
};


LUALIB_API int luaopen_auracore(lua_State *L)
{
	luaL_newmetatable(L, laura_node_type);
	luaL_setfuncs(L, node_meta, 0);

	luaL_newmetatable(L, laura_eventloop_type);
	luaL_setfuncs(L, loop_meta, 0);

	luaL_newlib(L, libfuncs);
	lua_setfield_int(L, "STATUS_OFFLINE", AURA_STATUS_OFFLINE);
	lua_setfield_int(L, "STATUS_ONLINE", AURA_STATUS_ONLINE);

	lua_setfield_int(L, "CALL_COMPLETED", AURA_CALL_COMPLETED);
	lua_setfield_int(L, "CALL_TIMEOUT", AURA_CALL_TIMEOUT);
	lua_setfield_int(L, "CALL_TRANSPORT_FAIL", AURA_CALL_TRANSPORT_FAIL);

	/* Return One result */
	return 1;
}
