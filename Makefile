.SUFFIXES:
-include blackjack.mk

#CC=clang --analyze -Xanalyzer -analyzer-output=text
CC=gcc

#Handle case when we're not cross-compiling
ifneq ($(GNU_TARGET_NAME),)
CROSS_COMPILE?=$(GNU_TARGET_NAME)-
endif

CFLAGS+=-Iinclude/ -g -Wall -fPIC
LDFLAGS+=-rdynamic -g

obj-y+= buffer.o buffer-dummy.o 
obj-y+= slog.o panic.o utils.o
obj-y+= transport.o aura.o export.o serdes.o retparse.o queue.o
obj-y+= transport-dummy.o
obj-y+= transport-serial.o
obj-y+= transport-usb.o usb-helpers.o
obj-y+= transport-susb.o bindings-lua.o 

define PKG_CONFIG
CFLAGS   += $$(shell pkg-config --cflags  $(1))
LDFLAGS  += $$(shell pkg-config --libs $(1))
INCFLAGS += $$(shell pkg-config --cflags-only-I $(1)) 
endef

$(eval $(call PKG_CONFIG,libusb-1.0))
$(eval $(call PKG_CONFIG,lua5.2))

all: libauracore.so test.usb test.dummy test.susb

libauracore.so: $(obj-y)
	$(SILENT_LD)$(CROSS_COMPILE)gcc -lusb-1.0 -O -shared -fpic -o $(@) $(^) $(LDFLAGS) 

cppcheck:
	cppcheck --enable=all \
	-Iinclude/ \
	-I/usr/lib/gcc/x86_64-linux-gnu/4.9/include \
	-I/usr/local/include \
	-I/usr/lib/gcc/x86_64-linux-gnu/4.9/include-fixed \
	-I/usr/include/x86_64-linux-gnu \
	-I/usr/include \
	$(INCFLAGS) $(obj-y:.o=.c) > /dev/null


checkpatch:
	./checkpatch.pl --no-tree -f $(obj-y:.o=.c) include/aura/*.h

test.usb: tests/test-usb.o $(obj-y)
	$(SILENT_LD)$(CROSS_COMPILE)$(CC) -o $(@) $(LDFLAGS) $(^)

test.susb: tests/test-susb.o $(obj-y)
	$(SILENT_LD)$(CROSS_COMPILE)$(CC) -o $(@) $(LDFLAGS) $(^)

test.dummy: tests/dummy-testcases.o $(obj-y)
	$(SILENT_LD)$(CROSS_COMPILE)$(CC) -o $(@) $(LDFLAGS) $(^)

%.o: %.c 
	$(SILENT_CC)$(CROSS_COMPILE)$(CC) $(CFLAGS) -c -o $(@) $(<)

clean:
	-rm *.o test.dummy test.usb libauracore.som
	-cd susb-test-fw && make mrproper
	-cd usb-test-fw && make mrproper
	-cd usb-test-dummy-fw && make mrproper

