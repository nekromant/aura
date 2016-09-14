#!/usr/bin/lua
package.cpath="./?.so;"..package.cpath
package.path="./../lua/?.lua;"..package.path

aura=require("aura")

function openshit()
	n = aura.open("someshit", "blah");
	if (n ~= nil) then
		exitcode = 1
	end
end

if pcall(openshit) then
	exitcode = 1
else
	exitcode = 0
end
