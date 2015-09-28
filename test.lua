#!/usr/bin/lua
package.cpath=package.cpath..";./lib?.so"
package.path=package.path..";./lua/?.lua"

aura = require("aura");
aura.slog_init(nil, 88);


node = aura.open("dummy", "./simpleusbconfigs/pw-ctl.conf");
print("----");
aura.wait_status(node, 1);


function callback(node, arg, ...)
   print("CALLBACK!")
   print(node, status, arg, ...);
end

node:__("echo_u16", callback, nil , 
	567);

node:echo_u8(5);

os.exit(1);

node:bit_set(bit32.lshift(12,8),1);
node:bit_set(bit32.lshift(13,8),1);
node:bit_set(bit32.lshift(14,8),1);

function loop()
   while true do 
      node:bit_set(bit32.lshift(12,8) + 1,1);
      node:bit_set(bit32.lshift(13,8) + 1,1);
      node:bit_set(bit32.lshift(14,8) + 1,1);
      
      node:bit_set(bit32.lshift(12,8),0);
      os.execute("sleep 1");
      node:bit_set(bit32.lshift(13,8),0);
      os.execute("sleep 1");
      node:bit_set(bit32.lshift(14,8),0);
      os.execute("sleep 1");
      node:bit_set(bit32.lshift(12,8),1);
      os.execute("sleep 1");
      node:bit_set(bit32.lshift(13,8),1);
      os.execute("sleep 1");
      node:bit_set(bit32.lshift(14,8),1);
      os.execute("sleep 1");
   end
end

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
