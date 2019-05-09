#include <ESP8266WiFi.h>

void readSys(unsigned long lastLoopTime, bool firstLoop){
  Serial.println("Read from sys");
  DataPoint dps[12];
  memset(dps, 0, sizeof(dps));
  unsigned long t = timeClient.getEpochTime();
  size_t n = 0;
  dps[n++] = createDataPoint(INT, "espVcc", "sys", ESP.getVcc());
  dps[n++] = createDataPoint(INT, "espFreeHeap", "sys", ESP.getFreeHeap());
  dps[n++] = createDataPoint(INT, "espHeapFragmentation", "sys", ESP.getHeapFragmentation());
  dps[n++] = createDataPoint(INT, "espWiFiRSSI", "sys", (int)WiFi.RSSI());
  dps[n++] = createDataPoint(INT, "lastLoopTime", "sys", lastLoopTime);
  dps[n++] = createDataPoint(INT, "espSketchSize", "sys", ESP.getSketchSize());
  
  float until = (int)(UPDATE_HOURS*60)-(otaInterval-1) % (int)(UPDATE_HOURS*60);
  dps[n++] = createDataPoint(INT, "secondsUntilNextUpdate", "sys", (int)until*60);

  if (firstLoop){
    dps[n++] = createDataPoint(BOOL, "firstLoop", "sys", firstLoop);
  }
  if (SPIFFS.exists("/data.dat")){
    File f = SPIFFS.open("/data.dat", "r");
    dps[n++] = createDataPoint(INT, "dataFileSize", "sys", f.size(), timeClient.getEpochTime());
    f.close();
  }
  bulkOutputDataPoints(dps, n, "sys", t);
}