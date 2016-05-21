local loop = { }

local aura = require("aura");


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
   self.EVTLOOP_ONCE = aura.EVTLOOP_ONCE;
   self.EVTLOOP_NONBLOCK = aura.EVTLOOP_NONBLOCK;
end

loop.add = function(self, node)
   aura.eventloop_add(self._handle, node)
   table.insert(self._nodes, node);
   node._eventloop = self;
end

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

loop.destroy = function(self)
   self._nodes = nil
   aura.eventloop_destroy(self._handle);
end

loop.dispatch = function(self, flags)
   return aura.eventloop_dispatch(self._handle, flags);
end

loop.loopexit = function(self, timeout_sec)
   return aura.eventloop_loopexit(self._handle, timeout_sec);
end

return loop;
