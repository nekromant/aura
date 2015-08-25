local aura = require("auracore");

aura.nodes = { }

aura.open = function(name, ...)
   local nhandle = aura.openfuncs[name](...);
   if (nil == nhandle) then
      return nil;
   end
   local node = require("aura/node")(aura, nhandle, {...});
   return node;
end

aura.eventloop = function(...)   
   return require("aura/loop")(aura, ...);
end

aura.dumpnodes = function()
   for n in pairs(aura.nodes) do
      print(n)
   end
end

aura.close = function(node) 
   aura.core_close(node._handle);
end

return aura