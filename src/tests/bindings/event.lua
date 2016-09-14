#!/usr/bin/lua
package.cpath=os.getenv("AURA_BUILDDIR").."/?.so;"..package.cpath
package.path=os.getenv("AURA_SOURCEDIR").."/lua/?.lua;"..package.path

aura = require("aura");
aura.slog_init(nil, 99);
n = aura.open("dummy", "blah");

exitcode = 1;

n.ping = function(self, arg)
    print("event", self, arg)
    exitcode = 0
end

l = aura.eventloop(n)

l:loopexit(3.45);
l:dispatch();
aura.close(n);

l = nil;
n = nil;
