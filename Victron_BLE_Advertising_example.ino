/*
  Initial BLE code adapted from Examples->BLE->Beacon_Scanner.
  Victron decryption code snippets from:
  
    https://github.com/Fabian-Schmidt/esphome-victron_ble

  Information on the "extra manufacturer data" that we're picking up from Victron SmartSolar
  BLE advertising beacons can be found at:
  
    https://community.victronenergy.com/storage/attachments/48745-extra-manufacturer-data-2022-12-14.pdf
  
  Thanks, Victron, for providing both the beacon and the documentation on its contents!
*/ 

#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// The Espressif people decided to use String instead of std::string in newer versions
// (3.0 and later?) of their ESP32 libraries. Check your BLEAdvertisedDevice.h file to see
// if this is the case for getManufacturerData(); if so, then uncomment this line so we'll
// use String code in the callback.  
#define USE_String


#include <aes/esp_aes.h>        // AES library for decrypting the Victron manufacturer data.


BLEScan *pBLEScan;

// The data in the BLE advertising broadcast from the SmartSolar device is encrypted by a 128-bit
// AES-CTR key. The key is created when you pair your SmartSolar device with the VictronConnect
// application; it's needed by this code in order to decrypt the BLE broadcast data.
//
// To obtain the key for your device, do the following (Apple iOS app; instructions for other
// platforms may differ):
//   1) If you haven't already, pair your SmartSolar device with the VictronConnect app.
//   2) Connect to the SmartSolar device in the application; you should see the device's "STATUS" page.
//   3) Touch the gear icon at the upper right to get to the "Settings" page.
//   4) Touch the three-vertical-dot icon at the upper right to get a popup menu; select "Product info".
//   5) Scroll to the bottom of the Product info page and ensure "Instant readout via Bluetooth" is enabled.
//   6) Touch the "SHOW" button in the "Encryption data" section; you'll get a popup that shows
//      the device's MAC address (informational; not used by this code) and the encryption key that
//      you need.
//   7) Touching the Encryption Key in the iOS app puts it into your paste buffer. Put it into a comment
//      in your source by pasting or by typing it manually. If typing it by hand, double-check your work
//      because this has to be EXACT (although case is unimportant).
//   8) Convert the hex string into an ESP32/C byte array by splitting it into two-character pairs, adding
//      commas and '0x' as appropriate, etc.
//
// Here's my copy-pasted Victron SmartSolar charge controller encryption key:
//
//   dc73cb155351cf950f9f3a958b5cd96f
//
// And then split the key into two-character pairs:
//
//   dc 73 cb 15 53 51 cf 95 0f 9f 3a 95 8b 5c d9 6f
//
// And finally, reformatted into the array definition needed by this code:
//
uint8_t key[16]={
    0xdc, 0x73, 0xcb, 0x15, 0x53, 0x51, 0xcf, 0x95,
    0x0f, 0x9f, 0x3a, 0x95, 0x8b, 0x5c, 0xd9, 0x6f
};

// Note: In my own (non-demo) code I paste the encryption key into a quoted character string
// and then use a function I wrote to convert it into the actual byte array. This saves me
// tedium and risk of mistakes in this reformat step. I didn't do that here so I could keep
// the code simple, so this is left as an excercise for the reader once you get things working.


int keyBits=128;  // Number of bits for AES-CTR decrypt.
int scanTime = 1; // BLE scan time (seconds)

char savedDeviceName[32];   // cached copy of the device name (31 chars max) + \0

// Victron docs on the manufacturer data in advertisement packets can be found at:
//   https://community.victronenergy.com/storage/attachments/48745-extra-manufacturer-data-2022-12-14.pdf
//


// Usage/style note: I use uint16_t in places where I need to force 16-bit unsigned integers
// instead of whatever the compiler/architecture might decide to use. I might not need to do
// the same with byte variables, but I'll do it anyway just to be at least a little consistent.


// Must use the "packed" attribute to make sure the compiler doesn't add any padding to deal with
// word alignment.
typedef struct {
  uint16_t vendorID;                    // vendor ID
  uint8_t beaconType;                   // Should be 0x10 (Product Advertisement) for the packets we want
  uint8_t unknownData1[3];              // Unknown data
  uint8_t victronRecordType;            // Should be 0x01 (Solar Charger) for the packets we want
  uint16_t nonceDataCounter;            // Nonce
  uint8_t encryptKeyMatch;              // Should match pre-shared encryption key byte 0
  uint8_t victronEncryptedData[21];     // (31 bytes max per BLE spec - size of previous elements)
  uint8_t nullPad;                      // extra byte because toCharArray() adds a \0 byte.
} __attribute__((packed)) victronManufacturerData;


// Must use the "packed" attribute to make sure the compiler doesn't add any padding to deal with
// word alignment.
typedef struct {
   uint8_t deviceState;
   uint8_t errorCode;
   int16_t batteryVoltage;
   int16_t batteryCurrent;
   uint16_t todayYield;
   uint16_t inputPower;
   uint8_t outputCurrentLo;             // Low 8 bits of output current (in 0.1 Amp increments)
   uint8_t outputCurrentHi;             // High 1 bit of output current (must mask off unused bits)
   uint8_t  unused[4];                  // Not currently used by Vistron, but it could be in the future.
} __attribute__((packed)) victronPanelData;

// FYI, here are Device State values. I haven't seen ones with '???' so I don't know
// if they exist or not or what they might mean:
//   0 = no charge from solar
//   1 = ???
//   2 = ???
//   3 = bulk charge
//   4 = absorption charge
//   5 = float
//   6 = ???
//   7 = equalization
// I've also seen a value '245' for about a second when my solar panel (simulated by a
// benchtop power supply) transitions from off/low voltage to on/higher voltage. There
// be others, but I haven't seen them.
