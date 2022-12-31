# arduino_i2c_orja
Danfoss heat pump USB based control interface, which mimics ThermIQ-USB functionality.

# Background

In 2015 I did a blog post where I reverse engineered the i2c protocol which Dandfoss and Thermia pumps are using (sorry post is in Finnish):

https://omakotikotitalomme.blogspot.com/2015/03/danfoss-lampopumpun-salaisuudet.html    

And finally I decided that its time to open source the source code and release updated gerber files which makes it easier to use (with Arduino Nano or Arduino pro mini). Hopefully this will allow cheap alternatives for controlling Danfoss heap pumps. Use at your own risk, don't blame me if you break your device. Always turn off the devices before connecting the i2c cable.    

I know that the code is not very beatiful and optimized, but it works. If you find bugs feel free to make merge request with a fix. Unfortunately I'm currently not able to give much support for this project, as I'm focusing on coding ESP32 based version which will add wifi and ROOM thermometer emulation support (similar functionality as MQTT based ThermIQ-ROOM2LP).

# Software alternatives for usage

Implementation has been tested with taloLogger, ThermIQ and ThermIQ2-Web. Please don't ask support from thermiq.net as this is not their hardware (so there is no official support for it) this just happends to mimic the same AT-command functionality. I've been running it 5+ years without problems. And of course you can use for example minicom to run AT-commands manually.

TaloLogger instructions:    
- https://olammi.iki.fi/sw/taloLoggerPi/howto.php     
- https://olammi.iki.fi/sw/taloLogger/

V1 version install instructions:    
- https://www.thermiq.net/wp-conteny/uploads/ThermIQ-installation-for-Raspberry-PI.pdf  (does not require license)

V2 version install instructions:    
- https://www.thermiq.net/en/sw/raspberry-pi/ (requires license which can be purchased from https://www.thermiq.net/en/produkt/thermiq2-web/ )

In theory it should also work with home assistant integration (but I haven't had time to test it):    
- https://github.com/tomrosenback/thermiq-node-red-homeassistant-config    
- https://github.com/ThermIQ/thermiq_mqtt-ha

# Building arduino hex firmware on Linux with Docker

You need to have Docker installed on your Linux machine and then run:

- ./linux_build.sh    

Resulting .hex files will be found under hex-directory after the build. You can use for example avrdude to flash it:    
- avrdude -v -p atmega328p -c arduino -P /dev/ttyUSB0 -b 57600 -D -U flash:w:i2c_rtc_simplified_with_watchdog_nano.ino.hex:i

# Building with Arduino IDE (Linux and Windows):
Install package depencies (Wire, NeoSWSerial and AltSoftSerial) before the build.

# Hardware with gerber pcb

Arduino Nano is prefered over Arduino Pro mini and serial ttl adapter, as its cheaper just to use Arduino Nano to power the device as compared to Pro mini and serial ttl adapter. Serial2 and Serial3 are optional adapters which can be used for example if you want to simultaneously use several serial connections with TaloLogger and ThermIQ web interfaces at the same time. Nano variant use 220uF capacitor to prevent rebooting when connecting to 1st serial port (unplug the capacitor during firmware flashing).

![Orja Gerber](images/orja_gerber.png?raw=true "Orja Gerber")

There is RO pins for ReadOnly mode (if you want to prevent write commands and are only gathering usage data).    
And there are serial speed controlling pins for switching between serial speeds (but most likely you want to stay at 9600 if you want to use the existing software backends).

There is a stl file for simple 3d printed screw-mount which will help protecting against short circuits against metal parts.

![Orja Mount](images/orja_stl_mount.jpg?raw=true "Orja Mount")
![Orja Mount2](images/orja_stl_mount2.jpg?raw=true "Orja Mount2")
![Orja Mount3](images/orja_stl_mount3.jpg?raw=true "Orja Mount3")

# BOM

BOM is around 10-20euroes depending where you source the hardware.

- Arduino Nano (recommended) or Arduino Pro mini (5V 16Mhz)    
- Serial ttl usb adapter(s) 5V (optional)    
- 4p dupont cable (for i2c connection)    
- 2.54mm header angle pins   
- 220uF capacitor for preventing Nano reseting when starting serial connection.
