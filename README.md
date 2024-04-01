# This project is cancelled for now
Note that I was finally not able to get this reliably working, especially with Windows 10 hosts and newer, I was always getting into trouble with reinitialization issues after resume from sleep or just synchronization issues due to the very tight timing dependencies of the software USB stack and also the PS/2 mouse reading code. I finally gave up and went back to improving [https://github.com/seife/marble_fx](https://github.com/seife/marble_fx) which is using an ATMega32u4 (Arduino Leonardo) to do the same thing and which works much more reliable for me. This alternate project is also (sort of...) maintained.

# USB converter for Trackman MarbleFX
The famous Logitech Trackman MarbleFX trackball has four buttons, of which the 4th (red) button was used to emulate a scroll wheel.
To enable this, the trackball needs to be switched into a special Logitech PS/2 protocol, which enables the reporting of the additional button.
This works well on Linux, but only if the trackball is connected via a native PS/2 port.

If a simple PS/2 <-> USB converter is used, the trackball operates in "legacy" 3-button Mouse mode, because those converters do not know how to switch and interpret the logitech protocol.

This project uses a Digispark ATTiny85 to interface the TrackmanFX's PS/2 port to USB HID.

The smallest boards can be integrated directly into the trackball, so that the TrackmanFX appears as a "native" USB device.

One example of such a modification is [here](doc/README.md).

The code can be built by simply loading it into the Arduino IDE and building it for Digispark. This will use the default DigiMouse library (which is based on V-USB).

Alternatively, you can build directly against upstream V-USB code and use the relatively small glue code in the "usbmouse" directory. To do this, run `prepare.sh` to download the current V-USB code from github, then `compile.sh` to use `arduino-cli` (which has to be in your `$PATH`) to build and link everything. This has the additional bonus of using a nicer device string in the USB descriptor ;-) If build succeeded, then you can use `upload.sh` to upload it to the Digispark.

V-USB is an excellent project brought to you by [Objective Develpment](https://www.obdev.at/vusb/).
