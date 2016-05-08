-- Consider aura a singleton
local aura = require("auracore");

aura.nodes = { }

local function copy (t)
    if type(t) ~= "table" then return t end
    local meta = getmetatable(t)
    local target = {}
    for k, v in pairs(t) do target[k] = v end
    setmetatable(target, meta)
    return target
end

aura.open = function(name, params)
   local mt = {}

   local node = aura.core_open(name, params);
   local node_tbl = { }

   mt.__index = function(self, idx)
      return self.__node[idx];
   end

   setmetatable(node_tbl, mt);
   node_tbl.__node = node;

   aura.set_node_containing_table(node, node_tbl);
   -- Store open node table here to avoid GC.
   -- GC will do a close() on node, but close is SYNC,
   -- This may make nasty stuff happen
   table.insert(aura.nodes, node_tbl);
   return node_tbl;
end

aura.eventloop = function(...)
   local loop = copy(require("aura/loop"))
   loop:init(...);
   loop.init = nil;
   return loop
end

aura.dumpnodes = function()
   for n in pairs(aura.nodes) do
      print(n)
   end
end

aura.close = function(node)
   aura.core_close(node.__node);
end

return aura
