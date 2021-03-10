#!/bin/bash
set -e
if [ -e v-usb ]; then
    ( cd v-usb; git pull --ff-only )
else
    ( git clone https://github.com/obdev/v-usb.git )
fi
[ -d lib ] || mkdir lib
cd lib
# we must not have additional stuff from v-usb in the libraries path
# so symlink only the necessary parts
rm -fr v-usb
mkdir v-usb
ln -s ../../v-usb/usbdrv v-usb
