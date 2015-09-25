#!/usr/bin/lua
package.cpath=package.cpath..";./lib?.so"
package.path=package.path..";./lua/?.lua"

aura = require("aura");
aura.slog_init(nil, 88);


node = aura.open_node("simpleusb", "./simpleusbconfigs/iceshard.conf");
aura.wait_status(node, 1);

function gpout(g,v)
   node:bit_set(bit32.lshift(g,8) + 1,1);
   node:bit_set(bit32.lshift(g,8),v);
end

gpout(0,1);
gpout(0,1);

gpout(12, 0);
   os.execute("sleep 0.5")
gpout(13, 0);
   os.execute("sleep 0.5")
gpout(14, 0);
   os.execute("sleep 0.5")

gpout(8, 0);
   os.execute("sleep 0.5")
gpout(9, 0);
   os.execute("sleep 0.5")


for i = 6100,2000,-250 do
   node:servo(i,0)
   os.execute("sleep 0.4")
end

gpout(12, 1);
gpout(13, 1);
gpout(14, 1);
gpout(8, 1);
gpout(9, 1);

os.exit(0);

gpout_one(12);
gpout_one(13);
gpout_one(14);

node:set_serial(0, 0, "sharhanas");
node:save(0,0)
os.exit(0);

while true do
   pcall(loop);
end

--node = aura.open("gpio", 0x1d50, 0x6032, "www.ncrmnt.org");
node = aura.open("simpleusb", "./simpleusbconfigs/pw-ctl.conf");

function cb(arg) 
   print("whoohoo");
end

node.ping = function(arg)
   print("ping",arg);
end

evloop = aura.eventloop(node)

--print(node:echo_u16(34));

--while true do
   evloop:handle_events(1500);
   --end
   evloop:handle_events(1500);
   evloop:handle_events(1500);

aura.close(node);

