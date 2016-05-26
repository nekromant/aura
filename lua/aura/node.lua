--- Represents an open aura node.
-- All functions exported by the remote device become members of this object
-- To call a remote function synchronously you 
-- @usage
-- -- Synchronous example
-- node = aura.open("dummy", nil);
-- aura.wait_status(node, aura.STATUS_ONLINE);
-- value = node:echo_u16(576);
-- @module node

require("aura/dumper");
local node = { };

--- 
-- Call a remote method asynchronously 
-- 
-- @tparam node self The node object 
-- @tparam string method The method name  
-- @tparam function callback The callback function to call 
-- @param callback_arg The user argument to callback function
--
-- @return 
--
node.__ = function(self, method, callback, callback_arg, ...)
   -- Just in case C library doesn't intercept the call
   error("Lua implementation of this method should not be executed")
end

return node; 