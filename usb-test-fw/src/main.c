#include <arch/antares.h>
#include <avr/boot.h>
#include <avr/io.h>
#include <string.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <generated/usbconfig.h>
#include <arch/vusb/usbportability.h>
#include <arch/vusb/usbdrv.h>

enum usb_requests { 
	RQ_GET_DEV_INFO,
	RQ_GET_OBJ_INFO,
	RQ_GET_EVENT,
	RQ_PUT_CALL
};

struct usb_interrupt_packet { 
	uint8_t pending_evts; 
} __attribute__((packed));

#define usbdev_is_be(pck) (pck & (1<<0))

struct usb_info_packet { 
	uint8_t  flags;
	uint16_t num_objs;
	uint16_t io_buf_size;
} __attribute__((packed));


int put_object(unsigned char *dest, int dlen, const void *name, const void* afmt, const void* rfmt)
{
	int tocopy; 
	int ret=0; 

        tocopy = min_t(int, dlen, strlen(name) + 1);
	memcpy(dest, name, tocopy);
	dest+=tocopy; 
	ret +=tocopy; 

        tocopy = min_t(int, dlen, strlen(afmt) + 1);
	memcpy(dest, afmt, tocopy);
	dest+=tocopy; 
	ret +=tocopy; 

        tocopy = min_t(int, dlen, strlen(rfmt) + 1);
	memcpy(dest, rfmt, tocopy);
	dest+=tocopy; 
	ret +=tocopy;
	return ret+1;
}

unsigned char iobuf[512];
static uint8_t num_evt_pending;

uchar   usbFunctionSetup(uchar data[8])
{
	usbRequest_t    *rq = (void *)data;
	switch(rq->bRequest) 
	{
	case RQ_GET_DEV_INFO: {
		struct usb_info_packet *p = (struct usb_info_packet *) iobuf;
		p->flags = 0;
		p->num_objs = 3;
		p->io_buf_size = 512;
		usbMsgPtr = iobuf;
		return sizeof(*p);
		break;
	};
	case RQ_GET_OBJ_INFO:
			usbMsgPtr = iobuf;
			iobuf[0]=1; /*isMethod ? */
			if (rq->wIndex.word == 0)
				return put_object(iobuf+1, 512, "turnTheLedOn", "1", "1");
			else if (rq->wIndex.word == 1)
				return put_object(iobuf+1, 512, "turnTheLedOff", "", "1");
			else {
				iobuf[0] = 0x0;
				return put_object(iobuf+1, 512, "buttonPressed", "", "1");
			}
			break;
	case RQ_GET_EVENT:
		
		break;
	case RQ_PUT_CALL:
		return USB_NO_MSG;
		break;

	}

	return 0;
}

uchar usbFunctionWrite(uchar *data, uchar len)
{
	num_evt_pending++;
	PORTC&=(1<<2);
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
	if(usbInterruptIsReady()){
		usbSetInterrupt(&num_evt_pending, 1);
	}

}
