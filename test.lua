#!/usr/bin/lua
package.cpath=package.cpath..";./lib?.so"
package.path=package.path..";./lua/?.lua"

aura = require("aura");
aura.slog_init(nil, 88);


dummy = aura.open("dummy", "./simpleusbconfigs/pw-ctl.conf");
node = aura.open("simpleusb", "./simpleusbconfigs/pw-ctl.conf");

l = aura.eventloop(node, dummy)

aura.wait_status(node, aura.STATUS_ONLINE);

function status_cb(node, status, arg)
   print("status cb", node, status, arg);
end

aura.status_cb(node, status_cb, 45);
aura.status_cb(dummy, status_cb, 45);


function callback(node, arg, ...)
   print("CALLBACK!")
   print(node, status, arg, ...);
end

n=1;

dummy.ping = function(self, farg)
   print("PING CB", farg)
   n = not n
end

dummy:__("echo_u16", callback, nil , 
	567);
node:__("bit_set", callback, nil, bit32.lshift(12,8),0);

l:handle_events();


node:echo_u8(5);

print("----");
collectgarbage();

aura.close(node);
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
      node:bit_set(bibt32.lshift(13,8),0);
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

node.ping = function(self, arg)
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
