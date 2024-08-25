void loop() {
  //Serial.println("Scanning...");

  BLEScanResults *foundDevices = pBLEScan->start(scanTime, false);
  
  pBLEScan->clearResults(); // delete results fromBLEScan buffer to release memory
  
}
