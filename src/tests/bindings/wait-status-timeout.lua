#!/usr/bin/lua
package.cpath=os.getenv("AURA_BUILDDIR").."/?.so;"..package.cpath
package.path=os.getenv("AURA_SOURCEDIR").."/lua/?.lua;"..package.path

aura = require("aura");
n = aura.open("dummy", "offline");

s = aura.wait_status(n, aura.STATUS_ONLINE, 1.5);
print(s,aura.STATUS_OFFLINE)
if (s ~= aura.STATUS_OFFLINE) then
    exitcode = 1
else
    exitcode = 0
end

collectgarbage();
