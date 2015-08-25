require("aura/class");
local node = class();

local function handle_status_change(self, newstatus, arg) 
   print("Status changed", self, newstatus, arg);
   self.__status = newstatus;
end

local function handle_etable_change(self, old, new, arg)
   print("Etable changed", self, old, new, arg);
   self.__etable = new;
end

local function handle_inbound(self, id, ...)
   print("Inbound on ".. self.__etable[id].name);
end

local function start_call(self, name, ...)
   
end

node.__init = function(self, aura, handle, args);
   self._handle = handle; 
   self._aura = aura;
   self._args = args; -- Args used to open the node
   aura.set_node_containing_table(handle, self);
   aura.status_cb(handle, handle_status_change, 5);
   aura.etable_cb(handle, handle_etable_change, 6);
   aura.event_cb(handle, handle_inbound, 6);
end

node.close = function(self)
   self._aura.close(self.handle);
   self = { } 
end

node.handle_events = function(self, timeout)
   
end

return node; 