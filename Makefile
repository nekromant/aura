.SUFFIXES:
-include blackjack.mk

#Handle case when we're not cross-compiling
ifneq ($(GNU_TARGET_NAME),)
CROSS_COMPILE?=$(GNU_TARGET_NAME)-
endif

CFLAGS+=-Iinclude/ -g -Wall -D_FORTIFY_SOURCE=2
LDFLAGS+=-rdynamic -g

obj-y+= buffer.o buffer-dummy.o 
obj-y+= slog.o panic.o utils.o
obj-y+= transport.o aura.o export.o serdes.o queue.o
obj-y+= transport-dummy.o
obj-y+= transport-serial.o
obj-y+= transport-usb.o usb-helpers.o

define PKG_CONFIG
CFLAGS  += $$(shell pkg-config --cflags  $(1))
LDFLAGS += $$(shell pkg-config --libs $(1))
endef

$(eval $(call PKG_CONFIG,libusb-1.0))

test: test.dummy test.usb
	./test.usb

test.usb: tests/test-usb.o $(obj-y)
	$(SILENT_LD)$(CROSS_COMPILE)gcc -o $(@) $(LDFLAGS) $(^)

test.dummy: tests/dummy-testcases.o $(obj-y)
	$(SILENT_LD)$(CROSS_COMPILE)gcc -o $(@) $(LDFLAGS) $(^)

%.o: %.c 
	$(SILENT_CC)$(CROSS_COMPILE)gcc $(CFLAGS) -c -o $(@) $(<)

clean:
	rm *.o test.dummy-