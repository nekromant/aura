.SUFFIXES:
-include blackjack.mk

CONFIG_TRANSPORT_NMC=n
CONFIG_BINDINGS_LUA=y
CONFIG_TRANSPORT_USB=y
CONFIG_TRANSPORT_SERIAL=y

SHELL:=/bin/bash
PREFIX?=/usr
LUA?=lua5.2
DESTDIR?=
LUA_PKG?=$(LUA)
LUA_CPATH?=$(shell $(LUA) lua-guess-lib-install-path.lua cpath $(PREFIX))
LUA_LPATH?=$(shell $(LUA) lua-guess-lib-install-path.lua path $(PREFIX))


CC?=clang
unit-tests  = $(shell ls tests/*.c)
dummy-tests = $(shell ls tests/dummy-*.c)

CFLAGS+=-Iinclude/ -g -Wall -fPIC
LDFLAGS+=-rdynamic -g -lrt

obj-y+= buffer.o buffer-dummy.o 
obj-y+= slog.o panic.o utils.o 
obj-y+= transport.o eventloop.o aura.o export.o serdes.o 
obj-y+= retparse.o queue.o utils-linux.o
obj-y+= eventsys-epoll.o
obj-y+= transport-dummy.o
obj-y+= transport-sysfs-gpio.o

obj-$(CONFIG_TRANSPORT_USB)+= transport-usb.o transport-susb.o usb-helpers.o
obj-$(CONFIG_BINDINGS_LUA)+= bindings-lua.o utils-lua.o
obj-$(CONFIG_TRANSPORT_SERIAL)+= transport-serial.o


define PKG_CONFIG
$$(info Fetching pkg-config for $(1))
CFLAGS   += $$(shell pkg-config --cflags  $(1))
LDFLAGS  += $$(shell pkg-config --libs $(1))
INCFLAGS += $$(shell pkg-config --cflags-only-I $(1)) 
endef

ifeq ($(CONFIG_TRANSPORT_USB),y)
$(eval $(call PKG_CONFIG,libusb-1.0))
endif

ifeq ($(CONFIG_BINDINGS_LUA),y)
$(eval $(call PKG_CONFIG,$(LUA_PKG)))
endif

ifeq ($(AURA_DISABLE_BACKTRACE),y)
CFLAGS+=-DAURA_DISABLE_BACKTRACE
endif

###########################################
# HACK: Fill in path to rcm kernel this to build easynmc transport
#
ifeq ($(CONFIG_TRANSPORT_NMC),y)
CFLAGS+=-Inmc-utils/include/ -DLIBEASYNMC_VERSION=\"0.1.1\"
CFLAGS+=-I/home/necromant/work/linux-mainline/include/uapi
CFLAGS+=-I/home/necromant/work/linux-mainline/drivers/staging/android/uapi
LDFLAGS+=-lelf
obj-y+=nmc-utils/easynmc-core.o
obj-y+=nmc-utils/easynmc-filters.o
obj-y+=transport-nmc.o ion.o
$(eval $(call PKG_CONFIG,libelf))
endif


all: libauracore.so $(subst .c,,$(unit-tests)) TAGS

libauracore.so: $(obj-y)
	$(SILENT_LD)$(CC) -O -shared -fpic -o $(@) $(^) $(LDFLAGS) 

libauracore.a: $(obj-y)
	$(SILENT_AR)$(AR) rcs $@ $^

define unit_test_rule
$(subst .c,,$(1)): $(subst .c,.o,$(1)) $$(obj-y)
	$$(SILENT_LD)$$(CC) -o $$(@) $$(^) $$(LDFLAGS)
endef


$(foreach u,$(unit-tests),$(eval $(call unit_test_rule,$(u))))

# Hint: Use gcc -E -x c++ - -v < /dev/nul to find out the paths

test: all
	echo "Running test suite"
	$(foreach u,$(dummy-tests),\
	valgrind --error-exitcode=1 --undef-value-errors=no \
		 --leak-check=full $(subst .c,,$(u)) && ) \
		echo "...Passed"

cppcheck:
	@echo "Running cppcheck, please standby..."
	@cppcheck --enable=all --force \
	-I/usr/include/c++/4.9 \
	-I/usr/include/x86_64-linux-gnu/c++/4.9 \
	-I/usr/include/c++/4.9/backward \
	-I/usr/lib/gcc/x86_64-linux-gnu/4.9/include \
	-I/usr/local/include \
	-I/usr/lib/gcc/x86_64-linux-gnu/4.9/include-fixed \
	-I/usr/include/x86_64-linux-gnu \
	-I/usr/include \
	$(INCFLAGS) $(obj-y:.o=.c) > /dev/null 2>cppcheck.log
	@cat cppcheck.log | grep -v "information" | grep -v "never used"

clang-analyze:
	make clean
	make CC="clang --analyze -Xanalyzer -analyzer-output=text"
infer: 
	make clean
	/opt/infer-linux64-v0.1.0/infer/infer/bin/infer -- make

checkpatch:
	./checkpatch.pl --no-tree -f $(obj-y:.o=.c) include/aura/*.h

%.o: %.c 
	$(SILENT_CC)$(CC) $(CFLAGS) -c -o $(@) $(<)

clean:
	-rm *.o test.dummy test.usb libauracore.so
	-cd susb-test-fw && make mrproper
	-cd usb-test-fw && make mrproper
	-cd usb-test-dummy-fw && make mrproper
	-rm tests/*.o

TAGS: $(obj-y)
	etags `find . \
        -name "*.c" -o -name "*.cpp" -o -name "*.h"`

doxygen: 
	-rm -Rfv doxygen/
	( cat Doxyfile ; echo "PROJECT_NUMBER=0.1" ) | doxygen 
	cd doxygen/html;\
	rm -Rfv .git;\
	git init .; git checkout --orphan gh-pages;\
	git add *;\
	git commit -m "documentation-for-gh-pages";\
	git remote add origin git@github.com:nekromant/aura.git;\
	git push -u -f origin gh-pages

install-check:
	@echo "lua interpreter: $(LUA)"
	@echo "Install prefix : $(PREFIX)"
	@echo "root dir:        $(DESTDIR)"
	@echo "lua .so dir:     $(LUA_CPATH)"
	@echo "lua lib dir:     $(LUA_LPATH)"


remove-lua:
	[ -d $(DESTDIR)/$(LUA_LPATH)/aura ] && rm -Rfv $(DESTDIR)/$(LUA_LPATH)/aura

define PC_FILE_TEMPLATE
prefix=$(PREFIX)
exec_prefix=$${prefix}
libdir=$${exec_prefix}/lib
includedir=$${prefix}/include/aura
sysconfdir=/etc

Name: libaura
Description: AURA RPC Library
Version: 0.1
Requires: lua5.2 libusb-1.0
Libs: -L$${libdir} -laura
Cflags: -I$${includedir}
endef
export PC_FILE_TEMPLATE

#FixMe: This install receipe is very naive
install-lua: install-lib
	[ -d $(DESTDIR)/$(LUA_CPATH) ] || mkdir -p $(DESTDIR)/$(LUA_CPATH)
	[ -d $(DESTDIR)/$(LUA_LPATH) ] || mkdir -p $(DESTDIR)/$(LUA_LPATH)
	cp -Rfv lua/* $(DESTDIR)/$(LUA_LPATH)
	ln -sf $(PREFIX)/lib/libaura.so $(DESTDIR)/$(LUA_CPATH)/auracore.so

install-lib: libauracore.so libauracore.a
	[ -d $(DESTDIR)/$(PREFIX)/lib/ ] || mkdir -p $(DESTDIR)/$(PREFIX)/lib/
	cp -f libauracore.so $(DESTDIR)/$(PREFIX)/lib/libaura.so
	cp -f libauracore.a  $(DESTDIR)/$(PREFIX)/lib/libaura.a

install-dev:
	[ -d $(DESTDIR)/$(PREFIX)/include/aura/ ] || mkdir -p $(DESTDIR)/$(PREFIX)/include/aura/
	[ -d $(DESTDIR)/$(PREFIX)/lib/pkgconfig/ ] || mkdir -p $(DESTDIR)/$(PREFIX)/lib/pkgconfig/
	cp -Rfv include/* $(DESTDIR)/$(PREFIX)/include/aura/
	$(SILENT_PKGCONFIG)echo "$$PC_FILE_TEMPLATE" > $(DESTDIR)/$(PREFIX)/lib/pkgconfig/aura.pc

install: install-lua install-lib install-dev

devtest: all
	cd nmc-utils && make clean && make
	scp ./nmc-utils/nmc-examples/aura-rpc/aura-test.abs root@192.168.20.9:
	scp tests/dummy-async root@192.168.20.9:
	scp tests/dummy-serdes-test root@192.168.20.9:
	scp tests/test-nmc root@192.168.20.9:
	scp nmc-utils/nmrun root@192.168.20.9:
	scp nmc-utils/nmctl root@192.168.20.9:
.PHONY: doxygen
