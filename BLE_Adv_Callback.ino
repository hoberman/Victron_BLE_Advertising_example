// Some understanding of this stuff might be found by digging around in the ESP32 library's
// file BLEAdvertisedDevice.h

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {

      #define manDataSizeMax 31     // BLE specs say no more than 31 bytes, but see comments below!


      // See if we have manufacturer data and then look to see if it's coming from a Victron device.
      if (advertisedDevice.haveManufacturerData() == true) {

        // Here's the thing: BLE specs say our manufacturer data can be a max of 31 bytes.
        // But: The library code puts this data into a String, which we will then copy to
        // a character (i.e., byte) buffer using String.toCharArray(). Assuming we have the
        // full 31 bytes of manufacturer data allowed by the BLE spec, we'll need to size our
        // buffer with an extra byte for a null terminator. Our toCharArray() call will need
        // to specify *32* bytes so it will copy 31 bytes of data with a null terminator
        // at the end. The real question is WHY does the BLE library code use String???
        uint8_t manCharBuf[manDataSizeMax+1];

        String manData = advertisedDevice.getManufacturerData(); // lib code returns String. Ugh.
        int manDataSize=manData.length(); // This does not count the null at the end.

        // Copy the data from the String to a byte array. Must have the +1 so we
        // don't lose the last character to the null terminator.
        manData.toCharArray((char *)manCharBuf,manDataSize+1);

        // Now let's setup a pointer to a struct to get to the data more cleanly.
        victronManufacturerData * vicData=(victronManufacturerData *)manCharBuf;

        // ignore this packet if the Vendor ID isn't Victron.
        if (vicData->vendorID!=0x02e1) {
          return;
        }

        // ignore this packet if it isn't type 0x01 (Solar Charger).
        if (vicData->victronRecordType != 0x01) {
          return;
        }

        // Not all packets contain a device name, so if we get one we'll save it and use it from now on.
        if (advertisedDevice.haveName()) {
          strcpy(savedDeviceName,advertisedDevice.getName().c_str());
        }
        
        if (vicData->encryptKeyMatch != key[0]) {
          Serial.printf("Packet encryption key byte 0x%2.2x doesn't match configured key[0] byte 0x%2.2x\n",
              vicData->encryptKeyMatch, key[0]);
          return;
        }

        uint8_t inputData[16];
        uint8_t outputData[16]={0};  // i don't really need to initialize the output.

        // The number of encrypted bytes is given by the number of bytes in the manufacturer
        // data as a while minus the number of bytes (10) in the header part of the data.
        int encrDataSize=manDataSize-10;
        for (int i=0; i<encrDataSize; i++) {
          inputData[i]=vicData->victronEncryptedData[i];   // copy for our decrypt below while I figure this out.
        }

        esp_aes_context ctx;
        esp_aes_init(&ctx);

        auto status = esp_aes_setkey(&ctx, key, keyBits);
        if (status != 0) {
          Serial.printf("  Error during esp_aes_setkey operation (%i).\n",status);
          esp_aes_free(&ctx);
          return;
        }
        
        // construct the 16-byte nonce counter array by piecing it together byte-by-byte.
        uint8_t data_counter_lsb=(vicData->nonceDataCounter) & 0xff;
        uint8_t data_counter_msb=((vicData->nonceDataCounter) >> 8) & 0xff;
        u_int8_t nonce_counter[16] = {data_counter_lsb, data_counter_msb, 0};
        
        u_int8_t stream_block[16] = {0};

        size_t nonce_offset=0;
        status = esp_aes_crypt_ctr(&ctx, encrDataSize, &nonce_offset, nonce_counter, stream_block, inputData, outputData);
        if (status != 0) {
          Serial.printf("Error during esp_aes_crypt_ctr operation (%i).",status);
          esp_aes_free(&ctx);
          return;
        }
        esp_aes_free(&ctx);

        // Now do our same struct magic so we can get to the data more easily.
        victronPanelData * victronData = (victronPanelData *) outputData;

        // Getting to these elements is easier using the struct instead of
        // hacking around with outputData[x] references.
        uint8_t deviceState=victronData->deviceState;
        uint8_t errorCode=victronData->errorCode;
        float batteryVoltage=float(victronData->batteryVoltage)*0.01;
        float batteryCurrent=float(victronData->batteryCurrent)*0.1;
        float todayYield=float(victronData->todayYield)*0.01*1000;
        float inputPower=float(victronData->inputPower)*0.01;

        // Getting the output current takes some magic because of the way they have the
        // 9-bit value packed into two bytes. The first byte has the low 8 bits of the count
        // and the second byte has the upper (most significant) bit of the 9-bit value plus some
        // There's some other junk in the remaining 7 bits - i'm not sure if it's useful for
        // anything else but we can't use it here! - so we will mask them off. Then combine the
        // two bye components to get an integer value in 0.1 Amp increments.
        int integerOutputCurrent=((victronData->outputCurrentHi & 0x01)<<9) | victronData->outputCurrentLo;
        float outputCurrent=float(integerOutputCurrent)*0.1;

        // I don't know why, but every so often we'll get half-corrupted data from the Victron. As
        // far as I can tell it's not a decryption issue because we (usually) get voltage data that
        // agrees with non-corrupted records.
        //
        // Towards the goal of filtering out this noise, I've found that I've rarely (or never) seen
        // corrupted data when the 'unused' bits of the outputCurrent MSB equal 0xfe. We'll use this
        // as a litmus test here.
        uint8_t unusedBits=victronData->outputCurrentHi & 0xfe;
        if (unusedBits != 0xfe) {
          return;
        }

        Serial.printf("%-31s  Battery: %6.2f Volts %6.2f Amps  Solar: %6.2f Watts Yield: %6.0f Wh  Load: %6.1f Amps  State: %2d\n",
          savedDeviceName,
          batteryVoltage, batteryCurrent,
          inputPower, todayYield,
          outputCurrent, deviceState
        );
      }
    }
};
