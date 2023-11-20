# Victron_BLE_Advertising_example
This is a minimal example on how to receive, decrypt, and decode Victron SmartSolar BLE advertising beacons.

I'm releasing this on Github to help others overcome the initial hurdles of getting data from 
advertising beacons.

Be aware that Espressif appears to be changing the types of some of the methods we use from std::string to
String. You'll need to check your build system's BLEAdvertisedDevice.h file and comment/uncomment the
USE_String #define accordingly.

Tested with:
	Sparkfun ESP32 Thing			ESP32-DOWDQ6
	M5 Stamp S3				ESP32-S3FN8
	M5 Stick C				ESP32-PICO
	Teyleten Robot ESP32S			ESP32-WROOM-32 Amazon - three for under $20
	Heltec/HiLetGo/MakerFocus ESP32		ESP32-S3 - has integrated OLED display

Notes:

I'm new to Github, so don't be surprised if you see wonky things going on here while I play with it.

I'm a hobbyist, not an experienced software developer, so be kind where I've made mistakes or 
done things in a weird way.

Yes, I've included my Victron's encryption key in my source code. I understand that's a bad practice, but
in this case I'm willing to accept the risk that you might drive by and be able to decode my SmartSolar
data. Oh, the horror!
