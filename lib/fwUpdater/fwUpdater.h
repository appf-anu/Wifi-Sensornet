#include <FS.h>
#include <Arduino.h>
#include <ESP8266httpUpdate.h>
#include <ESP8266HTTPClient.h> 
#include <WiFiClient.h>

 
#ifdef ARDUINO_ESP8266_NODEMCU
const char *fwVersionUrl = "http://xn--2xa.ink/files/nodemcuv2.bin.md5";
#elif defined ARDUINO_ESP8266_ESP12
const char *fwVersionUrl = "http://xn--2xa.ink/files/esp12e.bin.md5";
#elif defined ARDUINO_ESP8266_ESP01
const char *fwVersionUrl = "http://xn--2xa.ink/files/esp01.bin.md5";
#else
const char *fwVersionUrl = "http://xn--2xa.ink/files/firmware.bin.md5";
#endif

const char *fwUrlSpr = "http://xn--2xa.ink/files/firmware/%s.bin";
const char *fwCommandUrl = "http://xn--2xa.ink/files/firmware/%s.precmd";


void runPreCmd(const char *newFWVersion){
  /**
   * Run commands before updating
   * if a file exists in the firmware repo name <fwhash>.precmd, run the commands in this file
   * 
   * commands: 
   *  removedatafile -> removes the data file
   *  resetwifi -> resets the wifi connectivity (making the device require reconfiguration)
   * @param newFWVersion string for the firmware version.
   */
  char preCmdUrl[128];
  sprintf(preCmdUrl, fwCommandUrl, newFWVersion);
  Serial.printf("precmd, %s\n", preCmdUrl);
  delay(100);
  WiFiClient wifiClient;
  HTTPClient httpClient;
  httpClient.begin(wifiClient, preCmdUrl);
  
  int httpCode = httpClient.GET();
  if (httpCode == 404) {
    Serial.println("no precmd");
    httpClient.end();
    return;
  }
  if( httpCode != 200 ) {
    Serial.printf("update error %d\n", httpCode);
    httpClient.end();
    return;   
  }
  String preCommandsStr = httpClient.getString();

  httpClient.end();
  delay(1000);
  const char* preCommands = preCommandsStr.c_str(); 
  if (strstr(preCommands, "removedatafile") != NULL){
    Serial.println("precmd: rm data file");
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
    else Serial.println("SPIFFS cant be mounted");
  }
  if (strstr(preCommands, "resetwifi") != NULL){
    Serial.println("resetting wifi");
    // todo, fix this. will disconnect and then wontbadly.
    ESP.eraseConfig();
    // WiFi.disconnect(true);
  }
  
}

void checkForUpdates() {
  /**
   * Check for updates to the current firmware
   * 
   * uses a different url for updates based on the chip type defines:
   * ARDUINO_ESP8266_ESP01, ARDUINO_ESP8266_NODEMCU, ARDUINO_ESP8266_ESP12
   * 
   * checks for an md5 hash in a file on the server and then begins the update process for that hash
   * the hash file should contain only the hash. 
   * the update file should live at <domain>/files/firmware/<hash>.bin
   */
  if(WiFi.status() != WL_CONNECTED) {
    Serial.println("No wifi no update");
    return;
  }
  String fwVersionUrlStr = String(fwVersionUrl);
  Serial.printf( "Checking for firmware updates at %s\n", fwVersionUrl);
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
  
  Serial.printf( "Current md5:   \t%s\n", curFWVersion);
  Serial.printf( "Available md5: \t%s\n", newFWVersion);

  if( strstr(newFWVersion, curFWVersion) == NULL) {
    runPreCmd(newFWVersion);
    delay(1000);
    char fwUrl[256];
    sprintf(fwUrl, fwUrlSpr, newFWVersion);
    Serial.printf( "Updating from %s\n", fwUrl);
    Serial.flush();
    Serial.end();
    delay(1000);
    // using ESPHttpUpdate.update( wifiClient, fwUrl ) closes wifiClient.
    WiFiClient wifiClient2;
    t_httpUpdate_return ret = ESPhttpUpdate.update( wifiClient2, fwUrl );
    Serial.begin(9600);
    switch(ret) {
      case HTTP_UPDATE_OK:
        Serial.println("HTTP_UPDATE_OK");
        break;
      case HTTP_UPDATE_FAILED:
        Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());

        Serial.printf("FreeSketchSpace: %d", ESP.getFreeSketchSpace());
        break;
      case HTTP_UPDATE_NO_UPDATES:
        Serial.println("HTTP_UPDATE_NO_UPDATES");
        break;
    }
  } 
}
