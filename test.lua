#!/usr/bin/lua
package.cpath=package.cpath..";./lib?.so"
package.path=package.path..";./lib/?.lua"

aura = require("auracore");
aura.slog_init(nil, 88);

node = aura.openfuncs.usb(0x1d50, 0x6032, "www.ncrmnt.org");