#!/usr/bin/lua
package.cpath=package.cpath..";./lib?.so"
package.path=package.path..";./lua/?.lua"

aura = require("aura");
aura.slog_init(nil, 88);

node = aura.open("usb", 0x1d50, 0x6032, "www.ncrmnt.org");