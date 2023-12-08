void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println();
  Serial.println("Reset.");
  Serial.println();
  Serial.printf("Source file: %s\n",__FILE__);
  Serial.printf(" Build time: %s\n",__TIMESTAMP__);
  Serial.println();
  delay(1000);

  Serial.printf("Using encryption key: ");
  for (int i=0; i<16; i++) {
    Serial.printf(" %2.2x",key[i]);
  }
  Serial.println();
  Serial.println();
  Serial.println();
  
  strcpy(savedDeviceName,"(unknown device name)");

  // Code from Examples->BLE->Beacon_Scanner. This sets up a timed scan watching for BLE beacons.
  // During a scan the receipt of a beacon will trigger a call to MyAdvertisedDeviceCallbacks().
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99); // less or equal setInterval value

  Serial.println("setup() complete.");
}
