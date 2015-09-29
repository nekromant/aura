.SUFFIXES:
-include blackjack.mk

SHELL:=/bin/bash
PREFIX?=/usr
LUA?=lua5.2
DESTDIR?=
LUA_PKGCONFIG=lua5.2
LUA_CPATH?=$(shell $(LUA) lua-guess-lib-install-path.lua cpath $(PREFIX))
LUA_LPATH?=$(shell $(LUA) lua-guess-lib-install-path.lua path $(PREFIX))

CC?=clang
unit-tests  = $(shell ls tests/*.c)
dummy-tests = $(shell ls tests/dummy-*.c)

#Handle case when we're not cross-compiling
ifneq ($(GNU_TARGET_NAME),)
CROSS_COMPILE?=$(GNU_TARGET_NAME)-
endif

CFLAGS+=-Iinclude/ -g -Wall -fPIC
LDFLAGS+=-rdynamic -g

obj-y+= buffer.o buffer-dummy.o 
obj-y+= slog.o panic.o utils.o 
obj-y+= transport.o eventloop.o aura.o export.o serdes.o 
obj-y+= retparse.o queue.o utils-linux.o
obj-y+= eventsys-epoll.o
obj-y+= transport-dummy.o
obj-y+= transport-serial.o
obj-y+= transport-sysfs-gpio.o
obj-y+= transport-usb.o usb-helpers.o
obj-y+= transport-susb.o 
#Lua bindings
obj-y+= bindings-lua.o 


define PKG_CONFIG
CFLAGS   += $$(shell pkg-config --cflags  $(1))
LDFLAGS  += $$(shell pkg-config --libs $(1))
INCFLAGS += $$(shell pkg-config --cflags-only-I $(1)) 
endef

$(eval $(call PKG_CONFIG,libusb-1.0))
$(eval $(call PKG_CONFIG,lua5.2))

all: libauracore.so $(subst .c,,$(unit-tests)) TAGS

libauracore.so: $(obj-y)
	$(SILENT_LD)$(CROSS_COMPILE)gcc -lusb-1.0 -O -shared -fpic -o $(@) $(^) $(LDFLAGS) 

define unit_test_rule
$(subst .c,,$(1)): $(subst .c,.o,$(1)) $$(obj-y)
	$$(SILENT_LD)$$(CROSS_COMPILE)$$(CC) -o $$(@) $$(LDFLAGS) $$(^)
endef


$(foreach u,$(unit-tests),$(eval $(call unit_test_rule,$(u))))

# Hint: Use gcc -E -x c++ - -v < /dev/nul to find out the paths

test: all
	echo "Running test suite"
	$(foreach u,$(dummy-tests),\
	valgrind --show-leak-kinds=all --leak-check=full $(subst .c,,$(u)) && ) \
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
	$(SILENT_CC)$(CROSS_COMPILE)$(CC) $(CFLAGS) -c -o $(@) $(<)

clean:
	-rm *.o test.dummy test.usb libauracore.som
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

#FixMe: This install receipe is very naive
install-lua: install-lib
	[ -d $(DESTDIR)/$(LUA_CPATH) ] || mkdir -p $(DESTDIR)/$(LUA_CPATH)
	[ -d $(DESTDIR)/$(LUA_LPATH) ] || mkdir -p $(DESTDIR)/$(LUA_LPATH)
	cp -Rfv lua/* $(DESTDIR)/$(LUA_LPATH)
	ln -sf $(DESTDIR)/$(PREFIX)/lib/libauracore.so $(DESTDIR)/$(LUA_CPATH)/auracore.so

install-lib: libauracore.so
	cp -f libauracore.so $(DESTDIR)/$(PREFIX)/lib/

install: install-lua

.PHONY: doxygen
