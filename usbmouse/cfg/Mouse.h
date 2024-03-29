/*
 * Based on the digispark DigiMouse code
 * https://github.com/ArminJo/DigistumpArduino
 *
 * fixed to work with current obdev V-USB code by Stefan Seyfried
 */
#ifndef __DigiMouse_h__
#define __DigiMouse_h__
 
#define REPORT_SIZE 4

#include <Arduino.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/wdt.h>

extern "C" {
#include "usbdrv.h"
}
#include "usbconfig.h"

static const uchar *rt_usbHidReportDescriptor = NULL;
static uchar rt_usbHidReportDescriptorSize = 0;
static const uchar *rt_usbDeviceDescriptor = NULL;
static uchar rt_usbDeviceDescriptorSize = 0;

#define MOUSEBTN_LEFT_MASK		0x01
#define MOUSEBTN_RIGHT_MASK		0x02
#define MOUSEBTN_MIDDLE_MASK	0x04

typedef uint8_t byte;

/* What was most recently read from the controller */
static unsigned char last_built_report[REPORT_SIZE];

/* What was most recently sent to the host */
static unsigned char last_sent_report[REPORT_SIZE];

uchar		 reportBuffer[REPORT_SIZE];

/*
 * https://www.usb.org/sites/default/files/hid1_11.pdf 7.2.4, "remarks":
 * The recommended default idle rate (rate when the device is initialized)
 * is 500 milliseconds for keyboards (delay before first repeat rate) and
 * infinity for joysticks and mice. => 0 == infinity.
 */
#define DIGIMOUSE_DEFAULT_REPORT_INTERVAL 0
static unsigned char must_report = 0;
static unsigned char idle_rate = DIGIMOUSE_DEFAULT_REPORT_INTERVAL / 4; // in units of 4ms
// new minimum report frequency system:
static unsigned long last_report_time = 0;


char usb_hasCommed = 0;

const PROGMEM unsigned char mouse_usbHidReportDescriptor[] = { /* USB report descriptor */
	(unsigned char) 0x05, (unsigned char) 0x01,	// USAGE_PAGE (Generic Desktop)
	(unsigned char) 0x09, (unsigned char) 0x02,	// USAGE (Mouse)
	(unsigned char) 0xa1, (unsigned char) 0x01,	// COLLECTION (Application)
	(unsigned char) 0x09, (unsigned char) 0x01,	//		USAGE_PAGE (Pointer)
	(unsigned char) 0xa1, (unsigned char) 0x00,	//		COLLECTION (Physical)
	(unsigned char) 0x05, (unsigned char) 0x09,	//		USAGE_PAGE (Button)
	(unsigned char) 0x19, (unsigned char) 0x01,	//		USAGE_MINIMUM (Button 1)
	(unsigned char) 0x29, (unsigned char) 0x03,	//		USAGE_MAXIMUM (Button 3)
	(unsigned char) 0x15, (unsigned char) 0x00,	//		LOGICAL_MINIMUM (0)
	(unsigned char) 0x25, (unsigned char) 0x01,	//		LOGICAL_MAXIMUM (1)
	(unsigned char) 0x95, (unsigned char) 0x03,	//		REPORT_COUNT (3)
	(unsigned char) 0x75, (unsigned char) 0x01,	//		REPORT_SIZE (1)
	(unsigned char) 0x81, (unsigned char) 0x02,	//		INPUT (Data,Var,Abs)
	(unsigned char) 0x95, (unsigned char) 0x01,	//		REPORT_COUNT (1)
	(unsigned char) 0x75, (unsigned char) 0x05,	//		REPORT_SIZE (5)
	(unsigned char) 0x81, (unsigned char) 0x01,	//		Input(Cnst)
	(unsigned char) 0x05, (unsigned char) 0x01,	//		USAGE_PAGE(Generic Desktop)
	(unsigned char) 0x09, (unsigned char) 0x30,	//		USAGE(X)
	(unsigned char) 0x09, (unsigned char) 0x31,	//		USAGE(Y)
	(unsigned char) 0x15, (unsigned char) 0x81,	//		LOGICAL_MINIMUM (-127)
	(unsigned char) 0x25, (unsigned char) 0x7f,	//		LOGICAL_MAXIMUM (127)
	(unsigned char) 0x75, (unsigned char) 0x08,	//		REPORT_SIZE (8)
	(unsigned char) 0x95, (unsigned char) 0x02,	//		REPORT_COUNT (2)
	(unsigned char) 0x81, (unsigned char) 0x06,	//		INPUT (Data,Var,Rel)
	(unsigned char) 0x09, (unsigned char) 0x38,	//	Usage (Wheel)
	(unsigned char) 0x95, (unsigned char) 0x01,	//	Report Count (1),
	(unsigned char) 0x81, (unsigned char) 0x06,	//	Input (Data, Variable, Relative)
	(unsigned char) 0xc0,				// END_COLLECTION
	(unsigned char) 0xc0				// END_COLLECTION
};


#define USBDESCR_DEVICE					1

const unsigned char usbDescrDevice[] PROGMEM = {		/* USB device descriptor */
	18,				/* sizeof(usbDescrDevice): length of descriptor in bytes */
	USBDESCR_DEVICE,		/* descriptor type */
	0x01, 0x01, /* USB version supported */
	USB_CFG_DEVICE_CLASS,
	USB_CFG_DEVICE_SUBCLASS,
	0,				/* protocol */
	8,				/* max packet size */
	USB_CFG_VENDOR_ID,		/* 2 bytes */
	USB_CFG_DEVICE_ID,		/* 2 bytes */
	USB_CFG_DEVICE_VERSION,		/* 2 bytes */
#if USB_CFG_VENDOR_NAME_LEN
	1,				/* manufacturer string index */
#else
	0,				/* manufacturer string index */
#endif
#if USB_CFG_DEVICE_NAME_LEN
	2,				/* product string index */
#else
	0,				/* product string index */
#endif
#if USB_CFG_SERIAL_NUMBER_LENGTH
	3,				/* serial number string index */
#else
	0,				/* serial number string index */
#endif
	1,				/* number of configurations */
};


void buildReport(unsigned char *reportBuf) {
	if (reportBuf != NULL) {
		memcpy(reportBuf, last_built_report, REPORT_SIZE);
	}

	memcpy(last_sent_report, last_built_report, REPORT_SIZE);
}

void clearMove() {
	// because we send deltas in movement, so when we send them, we clear them
	last_built_report[1] = 0;
	last_built_report[2] = 0;
	last_built_report[3] = 0;
	last_sent_report[1] = 0;
	last_sent_report[2] = 0;
	last_sent_report[3] = 0;
}


class DigiMouseDevice {
 public:
	DigiMouseDevice () {
		rt_usbHidReportDescriptor = mouse_usbHidReportDescriptor;
		rt_usbHidReportDescriptorSize = sizeof(mouse_usbHidReportDescriptor);
		rt_usbDeviceDescriptor = usbDescrDevice;
		rt_usbDeviceDescriptorSize = sizeof(usbDescrDevice);
	}

	void begin() {
		cli();
		usbDeviceDisconnect();
		_delay_ms(200);
		usbDeviceConnect();

		usbInit();

		sei();
		last_report_time = millis();
	}

	void poll() {
		update();
	}

	void update() {
		usbPoll();

		// instead of above code, use millis arduino system to enforce minimum reporting frequency
		unsigned long time_since_last_report = millis() - last_report_time;
		/* idle_rate == 0, never send idle reports
		   cf https://www.usb.org/sites/default/files/hid1_11.pdf 7.2.4 "Set_Idle Request":
		   "When the upper byte of wValue is 0 (zero), the duration is indefinite. The endpoint will
		    inhibit reporting forever, only reporting when a change is detected in the report data." */
		if (idle_rate > 0 && time_since_last_report >= (idle_rate * 4 /* in units of 4ms - usb spec stuff */)) {
			last_report_time += idle_rate * 4;
			must_report = 1;
		}

		// if the report has changed, try force an update anyway
		if (memcmp(last_built_report, last_sent_report, REPORT_SIZE)) {
			must_report = 1;
		}

		// if we want to send a report, signal the host computer to ask us for it with a usb 'interrupt'
		if (must_report) {
			if (usbInterruptIsReady()) {
				must_report = 0;
				buildReport(reportBuffer); // put data into reportBuffer
				clearMove(); // clear deltas
				usbSetInterrupt(reportBuffer, REPORT_SIZE);
			}
		}
	}

	// delay while updating until we are finished
	void delay(long milli) {
		unsigned long last = millis();
		while (milli > 0) {
			unsigned long now = millis();
			milli -= now - last;
			last = now;
			update();
		}
	}

#if 0
	void moveX(char deltaX) {
		if (deltaX == -128) deltaX = -127;
		last_built_report[1] = *(reinterpret_cast<unsigned char *>(&deltaX));
	}

	void moveY(char deltaY) {
		if (deltaY == -128) deltaY = -127;
		last_built_report[2] = *(reinterpret_cast<unsigned char *>(&deltaY));
	}

	void scroll(char deltaS) {
		if (deltaS == -128) deltaS = -127;
		last_built_report[3] = *(reinterpret_cast<unsigned char *>(&deltaS));
	}
#endif
	void move(char deltaX, char deltaY, char deltaS) {
		if (deltaX == -128) deltaX = -127;
		if (deltaY == -128) deltaY = -127;
		if (deltaS == -128) deltaS = -127;
		last_built_report[1] = *(reinterpret_cast<unsigned char *>(&deltaX));
		last_built_report[2] = *(reinterpret_cast<unsigned char *>(&deltaY));
		last_built_report[3] = *(reinterpret_cast<unsigned char *>(&deltaS));
	}
#if 0
	void move(char deltaX, char deltaY, char deltaS, char buttons) {
		if (deltaX == -128) deltaX = -127;
		if (deltaY == -128) deltaY = -127;
		if (deltaS == -128) deltaS = -127;
		last_built_report[0] = buttons;
		last_built_report[1] = *(reinterpret_cast<unsigned char *>(&deltaX));
		last_built_report[2] = *(reinterpret_cast<unsigned char *>(&deltaY));
		last_built_report[3] = *(reinterpret_cast<unsigned char *>(&deltaS));
	}

	void rightClick(){
		last_built_report[0] = MOUSEBTN_RIGHT_MASK;
	}

	void leftClick(){
		last_built_report[0] = MOUSEBTN_LEFT_MASK;
	}

	void middleClick(){
		last_built_report[0] = MOUSEBTN_MIDDLE_MASK;
	}
#endif
	void setButtons(unsigned char buttons) {
		last_built_report[0] = buttons;
	}
#if 0
	void setValues(unsigned char values[]) {
		memcpy(last_built_report, values, REPORT_SIZE);
	}
#endif
};

// create the global singleton DigiMouse
DigiMouseDevice DigiMouse = DigiMouseDevice();

// USB_PUBLIC uchar usbFunctionSetup

uchar usbFunctionSetup(uchar data[8])
{
	usbRequest_t *rq = (usbRequest_t *)data;
	usbMsgPtr = reportBuffer;

	if ((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS) {		/* class request type */
		if (rq->bRequest == USBRQ_HID_GET_REPORT) {	 /* wValue: ReportType (highbyte), ReportID (lowbyte) */
			/* we only have one report type, so don't look at wValue */
			return REPORT_SIZE;
		} else if (rq->bRequest == USBRQ_HID_GET_IDLE) {
			usbMsgPtr = &idle_rate;
			return 1;
		} else if (rq->bRequest == USBRQ_HID_SET_IDLE) {
			idle_rate = rq->wValue.bytes[1];
		}
	} else {
		/* no vendor specific requests implemented */
	}
	return 0;
}

uchar usbFunctionDescriptor(struct usbRequest *rq)
{
	if ((rq->bmRequestType & USBRQ_TYPE_MASK) != USBRQ_TYPE_STANDARD) {
		return 0;
	}

	if (rq->bRequest == USBRQ_GET_DESCRIPTOR) {
		// USB spec 9.4.3, high byte is descriptor type
		switch (rq->wValue.bytes[1]) {
			case USBDESCR_DEVICE:
				usbMsgPtr = (usbMsgPtr_t)rt_usbDeviceDescriptor;
				return rt_usbDeviceDescriptorSize;
				break;

			case USBDESCR_HID_REPORT:
				usbMsgPtr = (usbMsgPtr_t)rt_usbHidReportDescriptor;
				return rt_usbHidReportDescriptorSize;
				break;
		}
	}

	return 0;
}

#endif // __DigiMouse_h__
