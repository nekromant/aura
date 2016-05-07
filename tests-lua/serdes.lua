#!/usr/bin/lua
package.cpath="./lib?.so;"..package.cpath
package.path="./../lua/?.lua;"..package.path


function test_echo(node, method, value)
    local ret = node[method](node, value);
    if (ret ~= value) then
        os.exit(1);
    end
end

aura = require("aura");
aura.slog_init(nil, 99);
n = aura.open("dummy", "blah");

aura.wait_status(n, aura.STATUS_ONLINE);
test_echo(n, "echo_u16", 128);
test_echo(n, "echo_i16", -128);
test_echo(n, "echo_i16", -128);


aura.close(n)
n = nil;
collectgarbage();
os.exit(0)
