local aura = require("auracore");

aura.open = function(name, ...)
   return aura.openfuncs[name](...);
end

return aura