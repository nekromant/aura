------------
-- The eventloop object.
-- This object represents an eventloop that is created by aura.eventloop()
-- @see aura.eventloop
-- @module eventloop

local loop = { }

local aura = require("aura");


--- Run event processing loop only for one iteration. 
loop.ONCE = 0;

--- Do not block, waiting for descriptors
loop.NONBLOCK = 0; 

-- TODO: Expose empty loop creation via bindings
loop.init = function(self, fnode, ...)
   self._nodes = { }
   self._handle = aura.eventloop_create(fnode)

   if (self._handle == nil) then
      error("Eventloop creation failed");
   end

   table.insert(self._nodes, fnode);
   fnode._eventloop = self;
   for i,j in ipairs({...}) do
      self:add(j);
   end
   self.ONCE = aura.EVTLOOP_ONCE;
   self.NONBLOCK = aura.EVTLOOP_NONBLOCK;
end




--- Add a node to this eventloop.
--
-- @tparam eventloop self The eventloop instance
-- @tparam node node The node to remove from this loop
--
loop.add = function(self, node)
   aura.eventloop_add(self._handle, node)
   table.insert(self._nodes, node);
   node._eventloop = self;
end

--- Deassociate a node from this eventloop.
--
--
-- @tparam eventloop self The eventloop instance
-- @tparam node node The node to remove from this loop
--
-- @return 
--
loop.del = function(self, node)
   for i,j in ipairs(self._nodes) do
      if (j==node) then
	 self._nodes[i] = nil
    node._eventloop = nil;
	 aura.eventloop_del(node);
	 return;
      end
   end
   error("Node removal from eventloop failed");
end

--- Destroy this eventloop. 
--
--  This will deassociate any nodes from this loop, but will not 
--  close them.  
-- 
-- 
-- @param self 
--
-- @return 
--
loop.destroy = function(self)
   self._nodes = nil
   aura.eventloop_destroy(self._handle);
end

---  Run the event processing loop and dispatch callbacks.
--
-- @tparam loop self eventloop instance
-- @tparam number flags bitmask of the following flags
--       aura.EVTLOOP_ONCE, aura.EVTLOOP_NONBLOCK or 0
--
--
loop.dispatch = function(self, flags)
   return aura.eventloop_dispatch(self._handle, flags);
end

--- Terminate the event processing loop.
-- The timeout is given in seconds, fractions of seconds are welcome. Zero timeout
-- or omitting the second argument will cause immediate eventloop termination.
--
--ProTIP: You can call this function BEFORE aura.eventloop_dispatch()
--
-- @tparam eventloop self The eventloop to terminate 
-- @param number timeout Terminate after timeout seconds 
--
-- @return 
--
loop.loopexit = function(self, timeout)
   return aura.eventloop_loopexit(self._handle, timeout);
end

return loop;
