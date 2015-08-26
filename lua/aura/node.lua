require("aura/class");
local node = class();

node.has_evtsys = false; 

local function handle_status_change(self, newstatus, arg) 
   print("Status changed", self, newstatus, arg);
   self.__status = newstatus;
end

local function handle_etable_change(self, old, new, arg)
   print("Etable changed", self, old, new, arg);

   for i,j in ipairs(new) do
      print(j.name);
   end

   self.__etable = new;
end

local function handle_inbound(self, id, ...)
   local name = self.__etable[id].name
   if (self[name] ~= nil) then
      self[name](...);
   else
      print("WARN: Unhandled event: ".. self.__etable[id].name);
   end
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

node.__handle_events = function(self, timeout)
   
end

return node; 