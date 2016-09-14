#include <stdio.h>
#include <stdlib.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <string.h>

void lua_stackdump(lua_State *L);

// Since the only way to send the exitcode from a lua script is os.exit()
// However doing this doesn't free any memory and valgrind will spit out
// kilometers of 'still-reachable' entries.
// This is a simplified wrapper that runs a lua script and properly frees
// everything. It also fetches the exitcode via a global variable exitcode
// and passes it to the shell.
// This way if we have something leaked - it's OUR problem, no excuses!

int main(int argc, char *argv[])
{
    lua_State *L = luaL_newstate();
    if (!L)
        return -1;

    char *scr;
    if (-1 == asprintf(&scr, "dofile('%s');", argv[1]))
        return -1;

    luaL_openlibs(L);

    int ret = luaL_loadbuffer(L, scr, strlen(scr), "ldr");
	lua_call(L, 0, 0);
	if (ret) {
        return -1;
	}

    lua_getglobal(L, "exitcode");
    ret = lua_tonumber(L, -1);
    lua_stackdump(L);
    lua_close(L);
    free(scr);

    return ret;
}
