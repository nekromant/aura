#include <arch/antares.h>
#include <avr/boot.h>
#include <avr/io.h>
#include <string.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <generated/usbconfig.h>
#include <arch/vusb/usbportability.h>
#include <arch/vusb/usbdrv.h>
#include <lib/urpc.h>

URPC_METHOD(setLed, 
	    URPC_U8,
	    URPC_U8)
{
	
};

URPC_METHOD(getButton, 
	    URPC_U8,
	    URPC_NONE)
{
	
};

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
unsigned char *bpos; 
int rq_len;
static uint8_t num_evt_pending=0;

uchar   usbFunctionSetup(uchar data[8])
{
	usbRequest_t    *rq = (void *)data;
	struct urpc_object **reg;
	int ocount = urpc_get_registry(&reg);

	usbMsgPtr = iobuf;
	switch(rq->bRequest) 
	{
	case RQ_GET_DEV_INFO: {
		struct usb_info_packet *p = (struct usb_info_packet *) iobuf;
		p->flags = 0;
		p->num_objs = ocount;
		p->io_buf_size = 512;
		usbMsgPtr = iobuf;
		return sizeof(*p);
		break;
	};
	case RQ_GET_OBJ_INFO:
	{
		struct urpc_object *o;
		if (rq->wIndex.word >= ocount)
			return;
		o = reg[rq->wIndex.word];
		
		iobuf[0]=urpc_object_is_method(o) ? 1 : 0; /*isMethod ? */
		return put_object(iobuf+1, 512, o->name, o->arg, o->ret);
	}		
	case RQ_GET_EVENT:
	{
		if (!num_evt_pending)
			return 0;
		uint16_t *id = iobuf; 
		uint8_t *ret = &iobuf[2];
		*id=0;
		*ret=7;
		PORTC^=(1<<2);
		num_evt_pending--;
		return 3;
		break;
	}
	case RQ_PUT_CALL:
		rq_len = rq->wLength;
		bpos=iobuf;
		return USB_NO_MSG;
		break;
	}
	return 0;
}

uchar usbFunctionWrite(uchar *data, uchar len)
{
	rq_len -= len;
	if (!rq_len)
		return 1;
	else
		return 0;
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
