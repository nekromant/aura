-- Guess lua library installation path
-- The algo is simple. We iterate over the package.path 
-- And look for the shortest string that contains the prefix

function split(inputstr, sep)
        if sep == nil then
                sep = "%s"
        end
        local t={} ; i=1
        for str in string.gmatch(inputstr, "([^"..sep.."]+)") do
                t[i] = str
                i = i + 1
        end
        return t
end

pth = arg[1]
prefix = arg[2];
triplet = arg[3];
if (nil ~= triplet) then
   triplet = triplet:gsub("%-", "%%-")
end
candidate = nil;

found_multiarch=false;

p = split(package[pth], ";");
for i,j in ipairs(p) do
   local t = split(j,"?")[1];
   if (nil ~= string.find(t, prefix)) then
      if (nil ~= triplet) and (nil ~= string.find(t, triplet)) then
	 candidate = t 
	 found_multiarch = true;
      elseif ((candidate == nil) or (#candidate > #t)) and not found_multiarch then
	 candidate = t
      end
   end
end

-- Just fail. 
if (nil == candidate) then
   os.exit(1);
end

candidate = candidate:gsub(prefix.."/", "")
io.write(candidate);
os.exit(0);
