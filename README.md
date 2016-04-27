| **Travis CI (Core Unit Tests)** | **Jenkins (transports, real hardware )** |
|------------------|----------------------|
| [![Build Status](https://travis-ci.org/nekromant/aura.svg?branch=master)](https://travis-ci.org/nekromant/aura) |   [![Build Status](https://jenkins.ncrmnt.org/job/GithubCI/job/aura/badge/icon?ts=1)](https://jenkins.ncrmnt.org/job/GithubCI/job/aura/)|

[![Coverage Status](https://coveralls.io/repos/github/nekromant/aura/badge.svg?branch=master)](https://coveralls.io/github/nekromant/aura?branch=master)

# AURA: Another Universal RPC, Actually

Aura is a simple and fast universal RPC library that allows you to quickly interface
with your favourite hardware without reinventing the wheel. Unlike most of the RPC
libraries, it is designed to link you to simple embedded devices and take care of their
weirdness, namely:

* It does all the job serializing and deserializing data, taking all the care of foreign endianness

* It doesn't use any complex data formats (e.g. XML) that some 8-bit avr will have a hard time
  parsing.

* Remote 'nodes' can go offline and online multiple times and it's not an error (unless you want it to be)

* Remote 'nodes' can even go offline and come back online with a different set of exported functions. And it
  still will not be an error, unless you want it to be.

* Aura provides hooks to override memory allocation, allowing you to interface with DSPs that require
  a special memory allocation technique for performance reasons, e.g. ION Memory Manager. And it allows
  you to avoid needless copying of serialized data. e.g. zero-copy

* Provides both sync and async API

* Written in pure C with as little dependencies as possible

* Has lua bindings (Want your favourite language supported as well - roll up the sleeves and contribute!)

* OpenSource

# TL; DR (In C, Sync API)

```C
int main() {
	int ret;
	struct aura_node *n = aura_open("dummy", NULL);
	aura_wait_status(n, AURA_STATUS_ONLINE); // Beware of TOCTOU! See docs!
	struct aura_buffer *retbuf;

	ret = aura_call(n, "echo_u16", &retbuf, 0x0102); //echo_u16 is a method on a remote device
	if (ret == 0) {
	   uint16_t rt = aura_buffer_get_u16(retbuf);
	   aura_buffer_release(n, retbuf);
           printf("Call result: %d\n", rt)
	}
	aura_close(n);
	return 0;
}
```

# TL; DR - 2 (In LUA, Sync API)

```lua
aura = require("aura");
node = aura.open("dummy", "blah");
aura.wait_status(node, aura.STATUS_ONLINE);
print(node:echo_u8(5))

```

For async examples see tests and docs.

# Installation

You'll need cmake, lua5.2-dev, libusb-1.0 and easynmc-0.1.1 (For NeuroMatrix Transport).

```
sudo apt-get install build-essential cmake lua5.2-dev libusb-1.0 easynmc-0.1.1
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr
make
sudo make install
```

Or just build yourself a debian package with
```
dpkg-buildpackage
```

# Terminology

Aura runs on a 'host machine'. E.g. Your laptop, Raspberry PI, OpenWRT router. Anything
with a recent linux ditro would do just fine.

Aura connects to different devices called 'nodes' via some transport. As you might've guessed this
needs a transport plugin. A full list of available plugins is somewhere below in this file.

When going online a node reports (in a transport-specific way) about stuff it has. It is called an
and aura fills in an 'export table'. Export table is just a list of objects that a node 'exports'.

Each object can be either a 'method' or 'event'. It is identified by it's name and a numeric id that is
used internally to issue a call.

A 'method' is just your good old RPC call, like:

result1, result2 = node.mymethod(arg1, arg2);

Host issues all method calls.

'Events' on the other hand are issued by a remote node. Normally you would want to use the async API for that!

Basically that's all you need to know. More details in the doxygen docs.

# Pull request checklist.

Before pull-requesting, please:

* Run static analysis on all of your code. Use facebook infer, clang-analyze and cppcheck. ALL OF THEM! They all all tend to find different things.

* If you've messed with aura core - run all of the test suite under valgrind to make sure no memory is leaking whatsoever. e.g.

```
mkdir build
cd build
cmake ..
make test
make ExperimatalMemcheck
```

* If you are adding a new transport - don't be lazy to add unit tests and some doxygen documentation.


* Transport plugin options and docs

All transport accept parameters as a string. The format of the string depends on the transport.


# Available transports:

* dummy - A dummy transport for unit tests. We love unit-tests. Nuff said!

* sysfs-gpio - A 'transport' that allows you to control host PC GPIO lines using the sysfs interface. Should serve as a more complex transport plugin example.  

* susb - A simple usb transport that does control transfers to a usb device according to a config file. Using this one you can interface with simplest possible USB devices that have no flash for any other logic. Doesn't support events. Needs lua, since the configuration file for it is just a lua script.

* usb - A more complex transport over USB. Supports events and methods, reads the information from the target device

* nmc - NeuroMatrix DSP transport. Requires experimental Easynmc Framework and ION Memory Manager. Aura must be running on a SoC equipped with a NeuroMatrix DSP, e.g. K1879ХБ1Я.

Want more? Contribute!

## simpleusb (a.k.a. susb)

simpleusb transport maps control transfers to methods. Simple as that. No events whatsoever. All methods require at least two arguments. wValue and wIndex (See USB spec). The transport accepts a path to the configuration file as it's option.

A typical configuration file may look like this:

```lua
device =
{
   vid = 0x1d50;
   pid = 0x6032;
   vendor  = "www.ncrmnt.org";
   product = "aura-susb-test-rig";
   --serial = "blah";
   methods = {
      [0] = NONE("led_ctl"),
      [1] = NONE("blink"),
      [2] = READ("read", UINT32, UINT32),
      [3] = WRITE("write", UINT32, UINT32),
   };
}


return device
```

The config above describes 4 methods:

```
--- Dumping export table ---
0. METHOD  led_ctl( uint16_t uint16_t )  [out 4 bytes] | [in 0 bytes]
1. METHOD  blink( uint16_t uint16_t )  [out 4 bytes] | [in 0 bytes]
2. METHOD  uint32_t uint32_t read( uint16_t uint16_t )  [out 12 bytes] | [in 8 bytes]
3. METHOD  write( uint16_t uint16_t uint32_t uint32_t )  [out 12 bytes] | [in 0 bytes]
-------------8<-------------

```

vendor, product and serial strings can be omited. If so the transport will match any.

NONE stands for a control packet with no data phase.

READ stands for *surprise* reading data from device

WRITE stands for writing data to device

Your hardware should pack the data without any padding between fields. E.g. Use  __attribute__(("packed")) for your structs in firmware.

N.B. 	   

VUSB-based devices (or libusb?) do not seem to play nicely when we have the setup packet only with no data part.
Transfers may fail instantly at random without any data delivered. A typical run at my box gives:

9122 succeeded, 878 failed total 10000

The distribution of failures is more or less even. See tests-transports/test-susb-stability-none

Adding just one byte of data to the packet fix the issue. Posible workarounds here are:

1. Retry N times

2. Add just one dummy byte for the transfer

Since nobody reported this workaround breaking support for their hardware (yet!) - we'll do it the second way.
For now.

## usb

This is a full-blown aura RPC over the usb bus with events.
The transport's option string looks like this:

"1d50:6032;vendorstring;productstring;serial"

To match *ANY* serial, product, vendor:

"1d50:6032;;;"


## nmc

You can use this transport to call remote methods running on RC Module's NeuroMatrix DSP core. This transport requires that you run it on a SoC with NeuroMatrix DSP and experimental easynmc driver, e.g. K1879ХБ1Я.

#TODO (For 0.3-release)
#### Better node timeout handling in eventloop code
#### Provide backend-agnostic event flags, do not rely on poll.h/epoll.h
#### Fix reconnect event delivery in susb code on libevent backend
#### Rework eventloop functions to be more libevent like
#### Implement proper serial port transport
#### Implement event delivery mechanism in Neuromatrix DSP transport
#### Make libevent backend default and stable
#### Refactor function naming (e.g. aura_open -> aura_node_open)
