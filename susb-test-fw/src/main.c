#include <arch/antares.h>
#include <avr/boot.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <generated/usbconfig.h>
#include <arch/vusb/usbportability.h>
#include <arch/vusb/usbdrv.h>

#include <lib/light_ws2812.h>

unsigned char iobuf[16];
struct cRGB target[2] = { 
        { 0xff, 0, 0 },
        { 0xff, 0, 0 }
};

struct cRGB current[2] = { 
        { 0x00, 0xff, 0 },
        { 0x00, 0xff, 0 }
};


uchar   usbFunctionSetup(uchar data[8])
{
	usbRequest_t    *rq = (void *)data;
	switch (rq->bRequest) { 
	case 0:
		target[0].r=rq->wValue.bytes[0];
		target[0].g=rq->wValue.bytes[1];
		target[0].b=rq->wValue.bytes[2];
		ws2812_setleds(&target, 2); 
		break;
	case 1:
		ws2812_setleds(&target, 2); 
		_delay_ms(rq->wValue.word);
		ws2812_setleds(&current, 2); 
		break;
	case 2: { 
		usbMsgPtr = iobuf;
		return 8;
	}
	case 3: 
		return USB_NO_MSG;
	}

	return 0;
}

uchar usbFunctionWrite(uchar *data, uchar len)
{
	memcpy(iobuf, data, len);
	return 1;
}

inline void usbReconnect()
{
	DDRD=0xff;
	_delay_ms(250);
	DDRD=0;
}

ANTARES_INIT_LOW(io_init)
{
	DDRC=1<<2;
	PORTC=0xff;
 	usbReconnect();
}

ANTARES_INIT_HIGH(uinit)
{
  	usbInit();
}


ANTARES_APP(usb_app)
{
	while (1) { 
		usbPoll();
	}
}
