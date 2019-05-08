#include <ESP8266WiFi.h>

void readSys(unsigned long lastLoopTime, bool firstLoop){
  Serial.println("Read from sys");
  DataPoint dps[8];
  memset(dps, 0, sizeof(dps));
  unsigned long t = timeClient.getEpochTime();
  dps[0] = createDataPoint(INT, "espVcc", "sys", ESP.getVcc());
  dps[1] = createDataPoint(INT, "espFreeHeap", "sys", ESP.getFreeHeap());
  dps[2] = createDataPoint(INT, "espHeapFragmentation", "sys", ESP.getHeapFragmentation());
  dps[3] = createDataPoint(INT, "espWiFiRSSI", "sys", (int)WiFi.RSSI());
  dps[4] = createDataPoint(INT, "lastLoopTime", "sys", lastLoopTime);
  
  float until = (int)(UPDATE_HOURS*60)-(tick-1) % (int)(UPDATE_HOURS*60);
  dps[5] = createDataPoint(INT, "secondsUntilNextUpdate", "sys", (int)until*60);
  size_t n = 6;
  if (firstLoop){
    dps[6] = createDataPoint(BOOL, "firstLoop", "sys", firstLoop);
    n++;
  }
  if (SPIFFS.exists("/data.dat")){
    File f = SPIFFS.open("/data.dat", "r");
    dps[6] = createDataPoint(INT, "dataFileSize", "sys", f.size(), timeClient.getEpochTime());
    f.close();
    n++;
  }
  bulkOutputDataPoints(dps, n, "sys", t);
}