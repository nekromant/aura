#!/usr/bin/lua
package.cpath=os.getenv("AURA_BUILDDIR").."/?.so;"..package.cpath
package.path=os.getenv("AURA_SOURCEDIR").."/lua/?.lua;"..package.path

aura = require("aura");
n = aura.open("dummy", "blah");
aura.close(n)

n = aura.open("dummy", "blah");
aura.wait_status(n, aura.STATUS_ONLINE)
aura.close(n)
n = nil;

collectgarbage();
exitcode = 0
