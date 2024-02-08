  // 1. Write 0x0000 to the CommStat register (0x61) two times in a row to unlock write protection.
  Serial.println("-> 1");
  Wire.beginTransmission(MG_ADDR);
  Wire.write(0x61);
  Wire.write(0x0);
  Wire.write(0x00);
  Serial.println(Wire.endTransmission());
  Wire.beginTransmission(MG_ADDR);
  Wire.write(0x61);
  Wire.write(0x0);
  Wire.write(0x00);
  Serial.println(Wire.endTransmission());

  // 2. Write desired memory locations to new values.
  Serial.println("-> 2");
  Wire.beginTransmission(NV_ADDR);
  Wire.write(0xB5);
  Wire.write(0x02);
  Wire.write(0x00);
  Serial.println(Wire.endTransmission());

  // 3. Write 0x0000 to the CommStat register (0x61) one more time to clear CommStat.NVError bit.
  Serial.println("-> 3");
  Wire.beginTransmission(MG_ADDR);
  Wire.write(0x61);
  Wire.write(0x00);
  Wire.write(0x00);
  Serial.println(Wire.endTransmission());

  // 4. Write 0xE904 to the Command register 0x060 to initiate a block copy.
  Serial.println("-> 4");
  Wire.beginTransmission(MG_ADDR);
  Wire.write(0x60);
  Wire.write(0x04);
  Wire.write(0xE9);
  Serial.println(Wire.endTransmission());

  // 5. Wait tBLOCK for the copy to complete.
  Serial.println("-> 5");
  delay(500);

  // 6. Check the CommStat.NVError bit. If set, repeat the process. If clear, continue.
  Serial.println("-> 6");
  Wire.beginTransmission(MG_ADDR);
  Wire.write(0x61);
  Serial.println(Wire.endTransmission());
  read_and_print(MG_ADDR, 0x61);

  // 7. Write 0x000F to the Command register 0x060 to send the full reset command to the IC.
  Serial.println("-> 7");

  // 8. Wait 10ms for the IC to reset. Write protection resets after the full reset command.
  Serial.println("-> 8");

  // 9. Write 0x0000 to the CommStat register (0x61) two times in a row to unlock write protection.
  Serial.println("-> 9");

  // 10. Write 0x8000 to the Config2 register 0x0AB to reset firmware.
  Serial.println("-> 10");

  // 11. Wait for the POR_CMD bit (bit 15) of the Config2 register to be cleared to indicate that the POR sequence is complete.
  Serial.println("-> 11");

  // 12. Write 0x00F9 to the CommStat register (0x61) two times in a row to lock write protection. 
  Serial.println("-> 12");
  Wire.beginTransmission(MG_ADDR);
  Wire.write(0x61);
  Wire.write(0xF9);
  Wire.write(0x00);
  Serial.println(Wire.endTransmission());
