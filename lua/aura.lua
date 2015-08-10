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

aura.newloop = function(...)
   for loop in ipairs({...}) do
      print(loop)
   end
end

aura.dumpnodes = function()
   for n in pairs(aura.nodes) do
      print(n)
   end
end

return aura