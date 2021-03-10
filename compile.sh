#!/bin/bash

arduino-cli compile \
	--build-property "build.extra_flags=-DUSE_OWN_USBMOUSE=1" \
	--libraries lib/v-usb,usbmouse \
	-b digistump:avr:digispark-tiny \
	--warnings all \
	-v \
	"$@"
