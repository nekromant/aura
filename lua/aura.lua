local aura = require("auracore");

function locals()
  local variables = {}
  local idx = 1
  while true do
    local ln, lv = debug.getlocal(2, idx)
    if ln ~= nil then
      variables[ln] = lv
    else
      break
    end
    idx = 1 + idx
  end
  return variables
end

aura.open = function(name, ...)
   return aura.openfuncs[name](...);
end

return aura