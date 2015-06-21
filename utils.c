#include <aura/aura.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

enum aura_endianness aura_get_host_endianness() {
        union {
                uint32_t i;
                char c[4];
        } e = { 0x01000000 };

        if (e.c[0])
                return AURA_ENDIAN_BIG;
        else
                return AURA_ENDIAN_LITTLE;
}


void aura_hexdump (char *desc, void *addr, int len) {
	int i;
	unsigned char buff[17];
	unsigned char *pc = (unsigned char*)addr;

	if (desc != NULL)
		printf ("%s:\n", desc);

	for (i = 0; i < len; i++) {
		if ((i % 16) == 0) {
			if (i != 0)
				printf ("  %s\n", buff);
			printf ("  %04x ", i);
		}

		printf (" %02x", pc[i]);
		if ((pc[i] < 0x20) || (pc[i] > 0x7e))
			buff[i % 16] = '.';
		else
			buff[i % 16] = pc[i];
		buff[(i % 16) + 1] = '\0';
	}

	while ((i % 16) != 0) {
		printf ("   ");
		i++;
	}

	printf ("  %s\n", buff);
}

void lua_stackdump (lua_State *L) {
	int i=lua_gettop(L);
	printf(" ----------------  Stack Dump ----------------\n" );
	while(  i   ) {
		int t = lua_type(L, i);
		switch (t) {
		case LUA_TSTRING:
			printf("%d:`%s'\n", i, lua_tostring(L, i));
			break;
		case LUA_TBOOLEAN:
			printf("%d: %s\n",i,lua_toboolean(L, i) ? 
			"true" : "false");
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
