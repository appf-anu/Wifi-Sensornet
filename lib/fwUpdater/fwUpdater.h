#include <Arduino.h>
#include <ESP8266httpUpdate.h>


const char *fwUrl = "http://xn--2xa.ink/files/firmware.bin";

void checkForUpdates() {
  String fwURL = String( fwUrl );
  String fwVersionURL = fwURL;
  fwVersionURL.concat( ".md5" );

  Serial.println( "Checking for firmware updates." );

  WiFiClient wifiClient;
  HTTPClient httpClient;
  httpClient.begin( wifiClient, fwVersionURL );
  int httpCode = httpClient.GET();
  if( httpCode == 200 ) {
    const char *newFWVersion = httpClient.getString().c_str();

    Serial.print( "Current firmware md5: " );
    Serial.println( ESP.getSketchMD5().c_str() );
    Serial.print( "Available firmware md5: " );
    Serial.println( newFWVersion );

    if( strstr(newFWVersion, ESP.getSketchMD5().c_str()) == NULL) {
      Serial.println( "Preparing to update" );

      t_httpUpdate_return ret = ESPhttpUpdate.update( wifiClient, fwUrl );

      switch(ret) {
        case HTTP_UPDATE_OK:
          Serial.println("HTTP_UPDATE_OK");
          break;
        case HTTP_UPDATE_FAILED:
          Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
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
