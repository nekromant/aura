[![Build Status](https://travis-ci.org/nekromant/aura.svg?branch=master)](https://travis-ci.org/nekromant/aura)

# AURA: Another Universal RPC, Actually

Aura is a simple and fast universal RPC library that allows you to quickly interface
with your favourite hardware without reinventing the wheel. Unlike most of the RPC
libraries, it is designed to link to simple embedded devices and take care of their 
weirdness, namely: 

* It does all the job serializing and deserializing data, taking care of foreign endianness

* It doesn't use any complex data formats (e.g. XML) that some 8-bit avr will have a hard time 
  parsing. 
 
* Remote 'nodes' can go offline and online multiple times and it's not an error (unless you want it to be)

* Aura provides hooks to override memory allocation, allowing you to interface with DSPs that require
  a special memory allocation technique for performance reasons, e.g. ION Memory Manager. 

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

# Available transports: 

* dummy - A dummy transport for unit tests. We love unit-tests. Nuff said!

* sysfs-gpio - A 'transport' that allows you to control host PC GPIO lines using the sysfs interface. Should serve as a more complex transport plugin example.  

* susb - A simple usb transport that does control transfers to a usb device according to a config file. Using this one you can interface with simplest possible USB devices that have no flash for any other logic. Doesn't support events. Needs lua, since the configuration file for it is just a lua script. 

* usb - A more complex transport over USB. Supports events and methods, reads the information from the target device

* nmc - NeuroMatrix DSP transport. Requires experimental Easynmc Framework and ION Memory Manager. Aura must be running on a SoC equipped with a NeuroMatrix DSP, e.g. K1879ХБ1Я.

Want more? Contribute!

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