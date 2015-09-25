-- Consider aura a singleton
local aura = require("auracore");

aura.nodes = { }

aura.open = function(name, params)
   local node = aura.open_node(name, params);
   local node_tbl = require("aura/node");
   table.insert(aura.nodes, node);
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
   node = nil;
end

aura.status = function(node)
   return aura.node_status(node._handle);
end

return aura