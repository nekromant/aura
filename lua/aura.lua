--- The main aura module
-- @module aura

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

--- Open a node.
--
-- This function opens a node using the specified transport and options
-- and returns the object to the caller. 
--
-- Before you can issue calls on this object you have to ensure it actually
-- goes online. 
--
-- To handle events on the object asynchronously you have to add it to an 
-- eventloop and call the event dispatching function
--
-- When you are done with this node, you can close it using aura.close()
--
-- @see aura.status
-- @see aura.status_cb
-- @see aura.eventloop
-- @see eventloop.dispatch
-- @see aura.close
-- @usage node = aura.open("dummy", "opts")
-- @tparam string name Transport name
-- @tparam string params Transport options, if any
--
-- @return The node object
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



---
-- Create an eventloop with one or more nodes assiciated with it.
-- This function accepts one or more nodes as parameters. 
--
-- @tparam node ... One or more nodes
-- @treturn eventloop eventloop instance created

aura.eventloop = function(...)
   local loop = copy(require("aura/loop"))
   loop:init(...);
   loop.init = nil;
   return loop
end

--- 
-- Dump some information about currently open nodes to stdout
--
-- @return 
--
aura.dumpnodes = function()
   for n in pairs(aura.nodes) do
      print(n)
   end
end

aura.close = function(node)
   aura.core_close(node.__node);
end

return aura
