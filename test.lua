#!/usr/bin/lua
package.cpath=package.cpath..";./lib?.so"
package.path=package.path..";./lua/?.lua"

aura = require("aura");
aura.slog_init(nil, 88);

node = aura.open("dummy", 0x1d50, 0x6032, "www.ncrmnt.org");
--node = aura.open("dummy");

aura.newloop("a","b","c")

function cb(arg) 
   print("whoohoo");
end



aura.close(node);

while true do
   aura.handle_events(node);
end