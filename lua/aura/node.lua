require("aura/class");
local node = class();

print(node);
node.__init = function(self, handle, args);
   print(unpack(args));
end


return node; 