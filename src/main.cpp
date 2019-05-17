#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <Arduino.h>
#include <Ticker.h>
#include <SPI.h>
#include <Wire.h>
#include <ConfigManager.h>
#include <ESP8266WiFi.h>
#include <fwUpdater.h>

#include <DNSServer.h>        //https://github.com/tzapu/WiFiManager
#include <ESP8266WebServer.h> //https://github.com/tzapu/WiFiManager
#include <WiFiManager.h>      //https://github.com/tzapu/WiFiManager

#include <NTPClient.h>        //https://github.com/arduino-libraries/NTPClient
#include <WiFiUdp.h>          //https://github.com/arduino-libraries/NTPClient

// DEFINES
#define DEBUG_POST false
#define DEBUG_WIFI_CONNECTION false
#define NO_STARTUP_UPDATE false
#define UPDATE_HOURS 2

#define DHTPIN D5
#define SCL D1
#define SDA D2

#define ALTITUDECONSTANT 577.0f

ADC_MODE(ADC_VCC);

// You can specify the time server pool and the offset (in seconds, can be
// changed later with setTimeOffset() ). Additionaly you can specify the
// update interval (in milliseconds, can be changed using setUpdateInterval() ).
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "au.pool.ntp.org", 0);
struct Config cfg;
static uint32_t otaInterval = 1;
bool shouldSaveConfig = false;
unsigned int sleepMinutes;
unsigned long sleepMicros;
unsigned int flashButtonCounter = 0;

// this includes include MUST BE AFTER TIMECLIENT AND CFG!!!
#include <DataManager.h>
// these includes must be after DataManager
#include <ReadChirp.h>
#include <ReadBME.h>
#include <ReadDHT.h>
#include <ReadBH1750.h>
#include <ReadSys.h>



//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void flashLed(){
  int state = digitalRead(LED_BUILTIN);  // get the current state of GPIO1 pin
  digitalWrite(LED_BUILTIN, !state);     // set pin to the opposite state
}
 
Ticker ticker;

void reset(){
  flashButtonCounter++;
  
  // need to mash the flash button
  if (flashButtonCounter < 5) {
    Serial.printf("press the flash button %d more times to ota next interval...\n", 5-flashButtonCounter);
    return;
  }
  
  ticker.attach(1.0/(float)flashButtonCounter, flashLed);
  otaInterval = 0; // reset the ota interval
  
  // need to mash the flash button
  if (flashButtonCounter < 10) {
    Serial.printf("press the flash button %d more times to reset datafile...\n", 10-flashButtonCounter);
    return;
  }
  ticker.attach(1.0/(float)flashButtonCounter, flashLed);
  SPIFFS.remove("/data.dat");
  if (flashButtonCounter < 15) {
    Serial.printf("press the flash button %d more times to reset config...\n", 15-flashButtonCounter);
    return;
  }
  Serial.println("resetting configuration....");
  ticker.attach(1.0/(float)flashButtonCounter, flashLed);
  // SPIFFS.remove("/config.json");
  
  WiFiManager wifiManager;
  wifiManager.resetSettings();
  WiFi.disconnect(true);
  Serial.println("Config reset, rebooting...");
  ESP.restart();
}

void setup() {
  Serial.println("mounting FS...");
  while (!SPIFFS.begin()) {
    Serial.println("failed to mount FS");
    delay(100);
  }
  pinMode(A0, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(0, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(0), reset, FALLING);

  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.println();

  //clean FS, for testing
  // SPIFFS.format();
  //read configuration from FS
  loadConfig(&cfg);

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_influxdb_server("server", "influxdb server", cfg.influxdb_server, 64);
  WiFiManagerParameter custom_influxdb_port("port", "influxdb port", cfg.influxdb_port, 6);
  WiFiManagerParameter custom_influxdb_db("database", "influxdb db", cfg.influxdb_db, 16);
  WiFiManagerParameter custom_influxdb_user("user", "username", cfg.influxdb_user, 16);
  WiFiManagerParameter custom_influxdb_password("password", "password", cfg.influxdb_password, 32);
  WiFiManagerParameter custom_interval("interval", "interval", cfg.interval, 4);
  WiFiManagerParameter custom_location("location", "location", cfg.location, 16);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //add all your parameters here
  wifiManager.addParameter(&custom_influxdb_server);
  wifiManager.addParameter(&custom_influxdb_port);
  wifiManager.addParameter(&custom_influxdb_db);
  wifiManager.addParameter(&custom_influxdb_user);
  wifiManager.addParameter(&custom_influxdb_password);
  wifiManager.addParameter(&custom_location);
  wifiManager.addParameter(&custom_interval);
  
  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //and goes into a blocking loop awaiting configuration
  char wifiName[17];
  sprintf(wifiName, "Sensornet %06x", ESP.getChipId());
  if (!wifiManager.autoConnect(wifiName)) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }
  WiFi.setAutoReconnect(true);
  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");


  //read updated parameters
  strcpy(cfg.influxdb_server, custom_influxdb_server.getValue());
  strcpy(cfg.influxdb_port, custom_influxdb_port.getValue());
  strcpy(cfg.influxdb_db, custom_influxdb_db.getValue());
  strcpy(cfg.influxdb_user, custom_influxdb_user.getValue());
  strcpy(cfg.influxdb_password, custom_influxdb_password.getValue());
  strcpy(cfg.location, custom_location.getValue());
  strcpy(cfg.interval, custom_interval.getValue());
  
  //save the custom parameters to FS
  if (shouldSaveConfig) {
    saveConfig(&cfg);
  }

  char hostname[32];
  sprintf(hostname, "ESP8266-%s-%d", cfg.location, ESP.getChipId());
  WiFi.hostname(hostname);

  timeClient.begin();
  Serial.println("Updating time...");
  timeClient.forceUpdate();
  Wire.setClockStretchLimit(2500);
  // i2c:   sda,scl
  // nodemcu D6,D7
  // esp8266 12,13
  // D2,D1
  Wire.begin(4,5);
  sleepMinutes = atoi(cfg.interval);
  sleepMicros  = sleepMinutes*6e+7;
#if ((!DEBUG_POST) && (!DEBUG_WIFI_CONNECTION) && (!NO_STARTUP_UPDATE))
  checkForUpdates();
#else
  Serial.println("NOT UPDATING ON BOOT!");
  Serial.printf("Sketch md5: %s\n", ESP.getSketchMD5().c_str());
#endif

}

double lastLoopTime = 0;
bool firstLoop = true; 
void loop() {
  ticker.detach();
  ticker.attach(0.05, flashLed);
  unsigned long startMicros = micros();

  // run update on second loop (tick+1)
  if (otaInterval % (unsigned int)((UPDATE_HOURS*60)/sleepMinutes) == 0){
    checkForUpdates();
  }else{
    Serial.printf("Not time for update %d/%d\n", otaInterval, (unsigned int)((UPDATE_HOURS*60)/sleepMinutes));
  }
  otaInterval ++;  
  for (byte address = 1; address < 127; address++){
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0){
      Serial.printf("Found I2C device at address 0x%02X\n", address);
      if (address == 0x20){
        readChirp();
      }
      if (address == 0x76 || address == 0x77) {
        if (!readBme280(address)) readBme680();
      }
      if (address == 0x23 || address == 0x5C){
        readBH1750(address);
      }
    }
  }
  
  readDHT();
  
  readSys(lastLoopTime, firstLoop);
  
  if (SPIFFS.exists("/data.dat") && WiFi.status() == WL_CONNECTED){
    DataPoint d;
    memset(&d, 0, sizeof(d));
    size_t readPos = 0;
    bool failedWrite = false;
    File f = SPIFFS.open("/data.dat", "r");
    size_t fileSize = f.size();
    f.close();
    while ((readPos = readDataPoint(&d, readPos)) != 0) {
      if (d.time == 0 || strcmp(d.name, "") == 0){
        Serial.println("breaking");
        break;
      }
      switch (d.type){
        case INT:
          Serial.printf("[%05d/%05d]-> %lu %s %d\n", readPos, fileSize, d.time, d.name, (int)d.value);
          break;
        case FLOAT:
          Serial.printf("[%05d/%05d]-> %lu %s %.2f\n", readPos, fileSize, d.time, d.name, (float)d.value);
          break;
        case DOUBLE:
          Serial.printf("[%05d/%05d]-> %lu %s %.2f\n", readPos, fileSize, d.time, d.name, (double)d.value);
          break;
        case BOOL:
          Serial.printf("[%05d/%05d]-> %lu %s %s\n", readPos, fileSize, d.time, d.name, ((bool)d.value)?"true":"false");
        break;
      }
      size_t tries = 0;
      do {
        delay(100);
      } while(tries++ < 3 && !postDataPointToInfluxDB(&d));
    }
    if(!failedWrite) SPIFFS.remove("/data.dat");
  }
  unsigned long delta = micros() - startMicros;
  if(delta >= sleepMicros){
      delta = sleepMicros;
    }
  #ifdef ESP_DEEPSLEEP
    ESP.deepSleep(sleepMicros-delta);
  #else
    firstLoop = false;
    unsigned long sleepTotal = (sleepMicros-delta)/1000;
    lastLoopTime = delta;
    Serial.printf("Delay for %.3fs\n", sleepTotal/(float)1000);
    ticker.detach();
    ticker.attach(1.0, flashLed);
    delay(sleepTotal);
    
  #endif
}