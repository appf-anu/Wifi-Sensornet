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

// https://github.com/esp8266/Arduino/blob/master/doc/exception_causes.rst

void readSys(unsigned long lastLoopTime, bool firstLoop){
  /**
   * Reads system statistics and reset info and adds them to a sender for transmission.
   */
  Serial.println("Read from sys");
  // DataPoint dps[4];
  // memset(dps, 0, sizeof(dps));
  // size_t n = 0;
  DataSender<DataPoint> sender(3);
  time_t t = time(nullptr);
  DataPoint vcc = createDataPoint(INT, "espVcc", "sys", ESP.getVcc(), t);
  sender.push_back(vcc);
  DataPoint espFreeHeap = createDataPoint(INT, "espFreeHeap", "sys", ESP.getFreeHeap(), t);
  sender.push_back(espFreeHeap);
  DataPoint espHeapFragmentation = createDataPoint(INT, "espHeapFragmentation", "sys", ESP.getHeapFragmentation(), t);
  sender.push_back(espHeapFragmentation);
  
  float until = (int)(UPDATE_HOURS*60)-(otaCounter-1) % (int)(UPDATE_HOURS*60);
  DataPoint secondsUntilNextUpdate = createDataPoint(INT, "secondsUntilNextUpdate", "sys", (int)until*60, t);
  sender.push_back(secondsUntilNextUpdate);
  DataPoint espSketchSize = createDataPoint(INT, "espSketchSize", "sys", ESP.getSketchSize(), t);
  sender.push_back(espSketchSize);
  DataPoint espWiFiRSSI = createDataPoint(INT, "espWiFiRSSI", "sys", (int)WiFi.RSSI(), t);
  sender.push_back(espWiFiRSSI);
  DataPoint llt = createDataPoint(INT, "lastLoopTime", "sys", lastLoopTime, t);
  sender.push_back(llt);
  
  if (firstLoop){
    rst_info *resetInfo = ESP.getResetInfoPtr();
    if (resetInfo != NULL){
      DataPoint reason = createDataPoint(INT, "rst_info.reason", "sys", resetInfo->reason, t);
      sender.push_back(reason);
      DataPoint exccause = createDataPoint(INT, "rst_info.exccause", "sys", resetInfo->exccause, t);
      sender.push_back(exccause);
      DataPoint epc1 = createDataPoint(INT, "rst_info.epc1", "sys", resetInfo->epc1, t);
      sender.push_back(epc1);
      
      DataPoint epc2 = createDataPoint(INT, "rst_info.epc2", "sys", resetInfo->epc2, t);
      sender.push_back(epc2);
      DataPoint epc3 = createDataPoint(INT, "rst_info.epc3", "sys", resetInfo->epc3, t);
      sender.push_back(epc3);
      DataPoint excvaddr = createDataPoint(INT, "rst_info.excvaddr", "sys", resetInfo->excvaddr, t);
      sender.push_back(excvaddr);
    }
  }


  // something wrong with reading flash sometimes.
  // FSInfo fs_info;
  // if (SPIFFS.info(fs_info)){
  //   dps[n++] = createDataPoint(INT, "fs_info.totalBytes", "sys", fs_info.totalBytes, t);
  //   dps[n++] = createDataPoint(INT, "fs_info.usedBytes", "sys", fs_info.usedBytes, t);
  // }
  
  // if (SPIFFS.exists("/data.dat")){
  //   File f = SPIFFS.open("/data.dat", "r");
  //   dps[n++] = createDataPoint(INT, "dataFileSize", "sys", f.size(), t);
  //   f.close();
  // }
  // delay(100);
  // bulkOutputDataPoints(dps, n, "sys", t);
}