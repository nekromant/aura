#include <arch/antares.h>
#include <avr/boot.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <generated/usbconfig.h>
#include <arch/vusb/usbportability.h>
#include <arch/vusb/usbdrv.h>


unsigned char iobuf[8];
uchar   usbFunctionSetup(uchar data[8])
{
	usbRequest_t    *rq = (void *)data;
	switch (rq->bRequest) { 
	case 0:
		PORTC&=~(1<<2);
		PORTC|=(rq->wValue.word << 2);
		break;
	case 1:
		PORTC&=~(1<<2);
		_delay_ms(rq->wValue.word);
		PORTC|=1<<2;
		break;
	case 2: { 
		uint32_t tmp = 0xdeadbeef;
		memcpy(iobuf, &tmp, sizeof(tmp));
		usbMsgPtr = iobuf;
		return 4;
	}
	}
	return 0;
}

uchar usbFunctionWrite(uchar *data, uchar len)
{

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
	usbPoll();
}
