-- Consider aura a singleton
local aura = require("auracore");

aura.nodes = { }

aura.open = function(name, params)
   local mt = {}

   local node = aura.open_node(name, params);
   local node_tbl = require("aura/node");

   mt.__index = function(self, idx)
      return self.__node[idx];
   end

   setmetatable(node_tbl, mt);   
   node_tbl.__node = node;

   aura.set_node_containing_table(node, node_tbl);

   return node_tbl;
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