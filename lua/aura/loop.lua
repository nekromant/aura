require("aura/class");
local loop = class();

loop.__init = function(self, aura, fnode, ...)
   self._aura   = aura;
   self._handle = aura.eventloop_create(fnode._handle)
   print(self._handle);
   if (self._handle == nil) then
      error("Eventloop creation failed");
   end

   for i,j in ipairs({...}) do
      aura.eventloop_add(j)
   end
end

loop.handle_events = function(self, timeout)
   self._aura.handle_events(self._handle, timeout);
end

return loop;