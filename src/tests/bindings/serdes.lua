#!/usr/bin/lua
package.cpath=os.getenv("AURA_BUILDDIR").."/?.so;"..package.cpath
package.path=os.getenv("AURA_SOURCEDIR").."/lua/?.lua;"..package.path

exitcode = 0


function test_echo(node, method, value)
    local ret = node[method](node, value);
    if (ret ~= value) then
        print("OOOPS: expected ".. value.. "got" .. ret .. "("..method..")")
        exitcode = exitcode + 1
    end
end

aura = require("aura");
aura.slog_init(nil, 99);
n = aura.open("dummy", "blah");


math.randomseed( os.time() )


aura.wait_status(n, aura.STATUS_ONLINE);

test_echo(n, "echo_u8", math.random(0, 255));
test_echo(n, "echo_i8", math.random(-125, 125));
test_echo(n, "echo_u16", math.random(0, 64000));
test_echo(n, "echo_i16", math.random(-30000, 30000));

test_echo(n, "echo_u32", math.random(0, 4294967296));
test_echo(n, "echo_i32", math.random(-2147483645, 2147483645));

-- Newer lua has problems dealing with huge 64-bit numbers
test_echo(n, "echo_u64", math.random(0, 0.9e+19))
test_echo(n, "echo_i64", math.random(0, 0.9e+19))

aura.close(n)
n = nil;
collectgarbage();
