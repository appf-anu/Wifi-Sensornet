
// CRC function used to ensure data validity
uint32_t calculateCRC32(const uint8_t *data, size_t length);


// Structure which will be stored in RTC memory.
// First field is CRC32, which is calculated based on the
// rest of structure contents.
// Any fields can go after CRC32.
// We use byte array as an example.
struct {
  uint32_t crc32started;
  uint32_t crc32sleptfor;
  time_t timeSleepStarted;
  uint64_t timeSleptFor;
} rtcData;

bool readRTCMem(time_t *theTime){
    if (ESP.rtcUserMemoryRead(0, (uint32_t*) &rtcData, sizeof(rtcData))) {
        uint32_t crc32started = calculateCRC32((uint8_t*) &rtcData.timeSleepStarted, sizeof(rtcData.timeSleepStarted));
        uint32_t crc32sleptfor = calculateCRC32((uint8_t*) &rtcData.timeSleptFor, sizeof(rtcData.timeSleptFor));
        
        if (crc32started != rtcData.crc32started || crc32sleptfor != rtcData.crc32sleptfor) {
            Serial.println("CRC32 in RTC memory doesn't match CRC32 of data. Data is probably invalid!");
            return false;
        }
        
        *theTime = (rtcData.timeSleepStarted + ((float)rtcData.timeSleptFor/1000000.0f));
        Serial.printf("Loaded time %d from RTC mem\n", *theTime);
        return true;
    }
    return false;
}

bool writeRTCData(time_t timeSleepStarted, uint64_t timeSleptFor){
    Serial.printf("Setting RTC mem to %d + %fs\n", timeSleepStarted, timeSleptFor/1000000.0f);
    rtcData.timeSleepStarted = timeSleepStarted;
    rtcData.timeSleptFor = timeSleptFor;
    rtcData.crc32started = calculateCRC32((uint8_t*) &rtcData.timeSleepStarted, sizeof(rtcData.timeSleepStarted));
    rtcData.crc32sleptfor = calculateCRC32((uint8_t*) &rtcData.timeSleptFor, sizeof(rtcData.timeSleptFor));
    if(ESP.rtcUserMemoryWrite(0, (uint32_t*) &rtcData, sizeof(rtcData))) return true;
    return false;
}

uint32_t calculateCRC32(const uint8_t *data, size_t length) {
  uint32_t crc = 0xffffffff;
  while (length--) {
    uint8_t c = *data++;
    for (uint32_t i = 0x80; i > 0; i >>= 1) {
      bool bit = crc & 0x80000000;
      if (c & i) {
        bit = !bit;
      }
      crc <<= 1;
      if (bit) {
        crc ^= 0x04c11db7;
      }
    }
    yield();
  }
  return crc;
}