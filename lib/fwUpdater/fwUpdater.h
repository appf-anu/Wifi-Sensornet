#include <FS.h>
#include <Arduino.h>
#include <ESP8266httpUpdate.h>
#include <ESP8266WiFi.h>

const char *fwVersionUrl = "http://xn--2xa.ink/files/firmware.bin.md5";
const char *fwUrlSpr = "http://xn--2xa.ink/files/firmware/%s.bin";
const char *fwCommandUrl = "http://xn--2xa.ink/files/firmware/%s.precmd";

void runPreCmd(HTTPClient *httpClient, const char *newFWVersion){
  WiFiClient wifiClient;
  char preCmdUrl[128];
  sprintf(preCmdUrl, fwCommandUrl, newFWVersion);
  Serial.printf("Running precmd, %s\n", preCmdUrl);
  delay(100);
  httpClient->begin( wifiClient, preCmdUrl );
  
  int httpCode = httpClient->GET();
  if( httpCode == 200 ) {
    String preCommandsStr = httpClient->getString();
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
      WiFi.disconnect(true);
    }
  }
  if (httpCode == 404) Serial.println("no precmds");
  Serial.println("Finished precmd");
}

void checkForUpdates() {
  String fwVersionUrlStr = String(fwVersionUrl);
  Serial.println( "Checking for firmware updates." );

  WiFiClient wifiClient;
  HTTPClient httpClient;
  httpClient.begin( wifiClient, fwVersionUrl );
  
  int httpCode = httpClient.GET();
  if( httpCode == 200 ) {
    String nVersionStr = httpClient.getString();
    nVersionStr.trim();
    const char *newFWVersion = nVersionStr.c_str();
    const char *curFWVersion = ESP.getSketchMD5().c_str();
    
    Serial.printf( "Current firmware md5: \t%s\n",  curFWVersion);
    Serial.printf( "Available firmware md5: \t%s\n", newFWVersion);

    if( strstr(newFWVersion, curFWVersion) == NULL) {
      runPreCmd(&httpClient, newFWVersion);
      
      char fwUrl[128];
      sprintf(fwUrl, fwUrlSpr, newFWVersion);
      Serial.printf( "Preparing to update from %s\n", fwUrl);
      t_httpUpdate_return ret = ESPhttpUpdate.update( wifiClient, fwUrl );

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
    }
    else {
      Serial.println( "Already on latest version" );
    }
  }
  else {
    Serial.print( "Firmware version check failed, got HTTP response code " );
    Serial.println( httpCode );
  } 
  httpClient.end();
}
