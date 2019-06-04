#include <ESP8266WiFi.h>
extern "C" {
#include <user_interface.h>
}

// enum rst_reason {
//     REASON_DEFAULT_RST      = 0,    /* normal startup by power on */
//     REASON_WDT_RST          = 1,    /* hardware watch dog reset */
//     REASON_EXCEPTION_RST    = 2,    /* exception reset, GPIO status won’t change */
//     REASON_SOFT_WDT_RST     = 3,    /* software watch dog reset, GPIO status won’t change */
//     REASON_SOFT_RESTART     = 4,    /* software restart ,system_restart , GPIO status won’t change */
//     REASON_DEEP_SLEEP_AWAKE = 5,    /* wake up from deep-sleep */
//     REASON_EXT_SYS_RST      = 6     /* external system reset */
// };

void readSys(unsigned long int t, unsigned long lastLoopTime, bool firstLoop){
  Serial.println("Read from sys");
  DataPoint dps[12];
  memset(dps, 0, sizeof(dps));
  size_t n = 0;
  dps[n++] = createDataPoint(INT, "espVcc", "sys", ESP.getVcc(), t);
  dps[n++] = createDataPoint(INT, "espFreeHeap", "sys", ESP.getFreeHeap(), t);
  dps[n++] = createDataPoint(INT, "espHeapFragmentation", "sys", ESP.getHeapFragmentation(), t);
  dps[n++] = createDataPoint(INT, "espWiFiRSSI", "sys", (int)WiFi.RSSI(), t);
  dps[n++] = createDataPoint(INT, "lastLoopTime", "sys", lastLoopTime, t);
  dps[n++] = createDataPoint(INT, "espSketchSize", "sys", ESP.getSketchSize(), t);
  
  float until = (int)(UPDATE_HOURS*60)-(otaCounter-1) % (int)(UPDATE_HOURS*60);
  dps[n++] = createDataPoint(INT, "secondsUntilNextUpdate", "sys", (int)until*60, t);

  if (firstLoop){
    dps[n++] = createDataPoint(INT, "bootReason", "sys", (int)ESP.getResetInfoPtr()->reason, t);
  }
  if (SPIFFS.exists("/data.dat")){
    File f = SPIFFS.open("/data.dat", "r");
    dps[n++] = createDataPoint(INT, "dataFileSize", "sys", f.size(), t);
    f.close();
  }
  bulkOutputDataPoints(dps, n, "sys", t);
}