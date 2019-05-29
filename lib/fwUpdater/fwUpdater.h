#include <FS.h>
#include <Arduino.h>
#include <ESP8266httpUpdate.h>
#include <ESP8266HTTPClient.h> 
#include <WiFiClient.h>

#ifdef ESP_DEEPSLEEP
const char *fwVersionUrl = "http://xn--2xa.ink/files/deepsleep-firmware.bin.md5";
#else
const char *fwVersionUrl = "http://xn--2xa.ink/files/firmware.bin.md5";
#endif
const char *fwUrlSpr = "http://xn--2xa.ink/files/firmware/%s.bin";
const char *fwCommandUrl = "http://xn--2xa.ink/files/firmware/%s.precmd";


void runPreCmd(const char *newFWVersion){
  char preCmdUrl[128];
  sprintf(preCmdUrl, fwCommandUrl, newFWVersion);
  Serial.printf("Running precmd, %s\n", preCmdUrl);
  delay(100);
  // WiFiClient wifiClient;
  HTTPClient httpClient;
  httpClient.begin(preCmdUrl);
  
  int httpCode = httpClient.GET();
  if (httpCode == 404) {
    Serial.println("no precmds");
    httpClient.end();
    return;
  }
  if( httpCode != 200 ) {
    Serial.printf("server error %d\n", httpCode);
    httpClient.end();
    return;   
  }
  String preCommandsStr = httpClient.getString();

  httpClient.end();
  delay(1000);
  const char* preCommands = preCommandsStr.c_str(); 
  if (strstr(preCommands, "removedatafile") != NULL){
    Serial.println("precmd: remove data file");
    size_t tries = 0;
    bool SPIFFS_began = false;
    while (!SPIFFS_began && tries < 3){
      SPIFFS_began = SPIFFS.begin();
      tries++;
    }
    if (SPIFFS_began){
      bool removed = SPIFFS.remove("/data.dat");
      Serial.println( removed ? "Successfully removed /data.dat" : "failed removing /data.dat");
    } 
    else Serial.println("SPIFFS couldnt be mounted");
  }
  if (strstr(preCommands, "resetwifi") != NULL){
    Serial.println("precmd: resetting wifi");
    // todo, fix this. will disconnect and then wontbadly.
    ESP.eraseConfig();
    // WiFi.disconnect(true);
  }
  Serial.println("Finished precmd");
  
}

void checkForUpdates() {
  String fwVersionUrlStr = String(fwVersionUrl);
  Serial.println( "Checking for firmware updates." );
  WiFiClient wifiClient;
  HTTPClient httpClient;
  httpClient.begin( wifiClient, fwVersionUrl );
  
  int httpCode = httpClient.GET();
  if( httpCode != 200 ) {
    Serial.printf( "Firmware version check failed, got HTTP response code %d\n" , httpCode);
    httpClient.end();
    return;
  }
  String nVersionStr = httpClient.getString();
  httpClient.end();

  nVersionStr.trim();
  const char *newFWVersion = nVersionStr.c_str();
  String curFwvers = ESP.getSketchMD5();
  const char *curFWVersion = curFwvers.c_str();
  
  Serial.printf( "Current firmware md5:   \t%s\n", curFWVersion);
  Serial.printf( "Available firmware md5: \t%s\n", newFWVersion);
  


  if( strstr(newFWVersion, curFWVersion) == NULL) {
    runPreCmd(newFWVersion);
    delay(1000);
    char fwUrl[256];
    sprintf(fwUrl, fwUrlSpr, newFWVersion);
    Serial.printf( "Preparing to update from %s\n", fwUrl);
    Serial.flush();
    Serial.end();
    delay(1000);
    // using ESPHttpUpdate.update( wifiClient, fwUrl ) closes wifiClient.
    WiFiClient wifiClient2;
    t_httpUpdate_return ret = ESPhttpUpdate.update( wifiClient2, fwUrl );
    Serial.begin(115200);
    switch(ret) {
      case HTTP_UPDATE_OK:
        Serial.println("HTTP_UPDATE_OK");
        break;
      case HTTP_UPDATE_FAILED:
        Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
        break;
      case HTTP_UPDATE_NO_UPDATES:
        Serial.println("HTTP_UPDATE_NO_UPDATES");
        break;
    }
  } else {
    Serial.println( "Already on latest version" );
  }  
}
