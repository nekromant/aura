function BIN(v) 
   return FMT_BIN..tostring(v).."."
end
		
function NONE(name)
   return { name, UINT16..UINT16, ""}
end

function WRITE(name,...)
   local fmt = UINT16..UINT16;
   for i,j in ipairs({...}) do
      fmt = fmt..j;
   end
   return { name, fmt, ""}
end

function READ(name,...)
   local fmt = "";
   for i,j in ipairs({...}) do
      fmt = fmt..j;
   end
   return { name, UINT16..UINT16, fmt}
end

device = dofile(simpleconf);
tbl = aura.etable_create(node, #device.methods + 1);
n = 0
while nil ~= device.methods[n] do
   aura.etable_add(tbl, unpack(device.methods[n]));
   n = n + 1
end

aura.etable_activate(tbl)
return device.vid, device.pid, device.vendor,device.product,device.serial