local aura = require("auracore");

aura.open = function(name, ...)
   return aura.openfuncs[name](unpack(arg));
end


return aura