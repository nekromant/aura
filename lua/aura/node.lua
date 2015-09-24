require("aura/class");
require("aura/dumper");
local node = class();

node.__current_exports = { };

local function handle_status_change(self, newstatus, arg) 
   print("Status changed", self, newstatus, arg);
   self.__status = newstatus;
end

local function start_call(self, name, ...)
   self._aura.start_call(self._handle, name, ...);
end


local function handle_etable_change(self, old, new, arg)
   print("Etable changed, repopulating", self, old, new, arg);

   dump(old);
   dump(new);
   self.__current_exports_named = { };

   for i,j in ipairs(new) do
      local name = j["name"];
      self.__current_exports_named[name] = j
      
      if (self[name] ~= nil) then
	 error("Ooops");
      end

      self[name] = function(self, ...)
	 self.__calldone = false;
	 start_call(self, j.id, ...);

	 while not self.__calldone do
	    evloop:handle_events(1500);
	 end

      end
   end
   self.__current_exports = new;   
end



local function is_event(obj)
   return (obj["arg"] == nil)
end

local function is_method(obj)
   return (obj["arg"] ~= nil)
end

local function handle_inbound(self, id, arg, ...)
   local o = self.__current_exports[id]
   local name = o.name

   if (is_event(o) and self[name] ~= nil) then
      self[name](...);
      return
   end
   
   if (is_method(o)) then
      if (self.__callresult == nil) then
	 print("OOOPS: Orphan call result?")
      end
      self.__callret = {...}
      self.__calldone = true;
      return;
   end
   error("Shit happened");
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

node.__ = function(name, callback, ...)
   local obj = self.__current_exports_named[name]
   obj.callback = callback
   start_call(self, obj.id, ...);
end

node.__handle_events = function(self, timeout)
   
end

return node; 