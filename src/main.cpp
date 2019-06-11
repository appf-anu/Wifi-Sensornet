// #define ESP_DEEPSLEEP true
#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <TimeManager.h>
#include <Arduino.h>
#include <Ticker.h>
#include <SPI.h>
#include <Wire.h>
#include <ConfigManager.h>

#include <time.h>
#include <sys/time.h>                   // struct timeval
#include <coredecls.h>                  // settimeofday_cb()
#include <sntp.h>

#include <ESP8266WiFi.h>      //https://github.com/tzapu/WiFiManager
#include <DNSServer.h>        //https://github.com/tzapu/WiFiManager
#include <ESP8266WebServer.h> //https://github.com/tzapu/WiFiManager
#include <WiFiManager.h>      //https://github.com/tzapu/WiFiManager

#include <NTPClient.h>        //https://github.com/arduino-libraries/NTPClient
#include <WiFiUdp.h>          //https://github.com/arduino-libraries/NTPClient

#include <fwUpdater.h>
// DEFINES
#define NO_STARTUP_UPDATE false
// chirp sensor clock stretching causes issues with esp8266.
#define USE_CHIRP false
#define UPDATE_HOURS 2

timeval cbtime;
#if defined ARDUINO_ESP8266_NODEMCU
  #define ONE_WIRE_PIN D5
  #define SCL D1
  #define SDA D2
#else
  #define ONE_WIRE_PIN 14
  #define SCL 5
  #define SDA 4
#endif
#define ALTITUDECONSTANT 577.0f

ADC_MODE(ADC_VCC);
unsigned long startMicros = 0;

// You can specify the time server pool and the offset (in seconds, can be
// changed later with setTimeOffset() ). Additionaly you can specify the
// update interval (in milliseconds, can be changed using setUpdateInterval() ).
// WiFiUDP ntpUDP;
// NTPClient timeClient(ntpUDP, "au.pool.ntp.org", 0);

struct Config cfg;
static uint32_t otaCounter = 1;
bool shouldSaveConfig = false;
unsigned int sleepMinutes;
unsigned long sleepMicros;
unsigned int flashButtonCounter = 0;

// this include MUST BE AFTER TIMECLIENT AND CFG!!!
#include <DataManager.h>
// these includes must be after DataManager
#include <ReadChirp.h>
#include <ReadBME.h>
#include <ReadDHT.h>
#include <ReadBH1750.h>
#include <ReadSys.h>
#include <ReadDallas.h>
#include <ReadHDC1000.h>

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

bool flashState = false;
void flashLed(){
  // lets not flash anything 
  flashState = !flashState;
  digitalWrite(2, flashState);
}

Ticker ticker;

// this needs to have this see https://github.com/esp8266/Arduino/issues/4468
void ICACHE_RAM_ATTR reset(){
  flashButtonCounter++;
  
  // need to mash the flash button
  if (flashButtonCounter < 5) {
    Serial.printf("press the flash button %d more times to ota next interval...\n", 5-flashButtonCounter);
    return;
  }
  
  ticker.attach(1.0/(float)flashButtonCounter, flashLed);
  otaCounter = 0; // reset the ota interval
  
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


void timecfg(){

  Serial.println("Updating time...");
  time_t tm = time(nullptr);
  Serial.printf("time %d\n", tm);
  Serial.println("configTime");
  configTime(0,0, "0.au.pool.ntp.org", "1.au.pool.ntp.org", "2.au.pool.ntp.org");
  delay(300);

}

void ICACHE_RAM_ATTR time_is_set(void){
  Serial.printf("Time is set %d\n", time(nullptr));
}

void setup() {
  startMicros = 0;
  settimeofday_cb(time_is_set);
  
    // set up TZ string to use a POSIX/gnu TZ string for local timezone
  // TZ string information:
  // https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
  
  Serial.println("resetting timezone");
  setenv("TZ", "UTC", 1);
  tzset(); // save the TZ variable

  timezone tz = { 0, 0};
  timeval tv = { 0, 0};
  time_t tm;
  if (readRTCMem(&tm)){
    Serial.printf("rtc-time %d\n", tm);
    tv = {tm, 0};
  }
  // DO NOT attempt to use the timezone offsets
  // The timezone offset code is really broken.
  // if used, then localtime() and gmtime() won't work correctly.
  // always set the timezone offsets to zero and use a proper TZ string
  // to get timezone and DST support.

  // set the time of day and explicitly set the timezone offsets to zero
  // as there appears to be a default offset compiled in that needs to
  // be set to zero to disable it.
  settimeofday(&tv, &tz);

  Serial.println("mounting FS...");
  while (!SPIFFS.begin()) {
    Serial.println("failed to mount FS");
    delay(100);
  }
  pinMode(A0, INPUT);

  
  pinMode(2, OUTPUT); // "esp led" DONT use LED_BUILTIN because it is used for deepsleep
  pinMode(0, INPUT_PULLUP); // aka D3 aka flash button
  attachInterrupt(0, reset, FALLING); // attach interrupt
  
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.println();

  //clean FS, for testing
  // SPIFFS.format();
  //read configuration from FS
  loadConfig(&cfg);
  // not sure if this is causing weird reboots.
  // WiFiClient client;
  // client.setDefaultNoDelay(true);
  // client.setNoDelay(true);

#ifdef ESP_DEEPSLEEP
  WiFi.reconnect();
  size_t tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries++ < 100) { // wait 10s for wifi.
    delay(100);
    Serial.print(".");
  }
#else

  WiFi.setSleepMode(WIFI_NONE_SLEEP);

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
  
  sprintf(wifiName, "Sensornet %06x\n", ESP.getChipId());
  Serial.printf("Wifi up on  %s", wifiName);
  //sets timeout until configuration portal gets turned off and esp gets reset.
  wifiManager.setTimeout(300);
  if (!wifiManager.autoConnect(wifiName)) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    // restart and try again, or maybe put it to deep sleep
    ESP.restart();
    delay(5000);
  }

  Serial.println();
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
#endif


  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  char hostname[32];
  sprintf(hostname, "ESP8266-%s-%d", cfg.location, ESP.getChipId());
  WiFi.hostname(hostname);

  // timeClient.begin();
  if (WiFi.status() == WL_CONNECTED){
    Serial.print("Station connected normal...");
    timecfg();
  }
#if USE_CHIRP
  // TODO: I think this is breaking the esps
  Wire.setClockStretchLimit(2500);
#endif
  // i2c:   sda,scl
  // 4, 5
  // D2,D1
  Wire.begin(SDA,SCL);
  sleepMinutes = atoi(cfg.interval);
  sleepMicros  = sleepMinutes*6e+7;
#if !NO_STARTUP_UPDATE
  checkForUpdates();
#else
  Serial.println("NOT UPDATING ON BOOT!");
  Serial.printf("Sketch md5: %s\n", ESP.getSketchMD5().c_str());
#endif
  
}


double lastLoopTime = 0;
bool firstLoop = true; 

void loop() {
#ifndef ESP_DEEPSLEEP
  startMicros = micros();
#endif
  
  flashButtonCounter = 0;
  ticker.detach();
  ticker.attach(0.05, flashLed);


  // run update on second loop (tick+1)
  if (otaCounter % (unsigned int)((UPDATE_HOURS*60)/sleepMinutes) == 0){
    checkForUpdates();
  }else{
    Serial.printf("Not time for update %d/%d\n", otaCounter, (unsigned int)((UPDATE_HOURS*60)/sleepMinutes));
  }
  otaCounter ++;  
  
  time_t tm = time(nullptr);
  // before time is jan 1 2000
  if (tm > 946684800 && tm < 3559717660){ // very high number to prevent some weirdness
    Serial.printf("Current time: %d\n", tm);

    for (byte address = 1; address < 127; address++){
      Wire.beginTransmission(address);
      if (Wire.endTransmission() == 0){
        Serial.printf("Found I2C device at address 0x%02X\n", address);
        if (address == 0x20){
#if USE_CHIRP
          readChirp(tm);
#else
          Serial.println("Not compiled to read from chirp sensor.");
#endif
        }

        if (address == 0x76 || address == 0x77) {
          if (!readBme280(tm,address)) readBme680(tm);
        }

        if (address == 0x40){
          readHDC(tm, address);
        }

        if (address == 0x23 || address == 0x5C){
          readBH1750(tm, address);
        }
      }
      yield();
    }
    readDHT(tm);
    delay(20);
    readDallas(tm);
    delay(20);
    readSys(tm, lastLoopTime, firstLoop);
    delay(20);
  }
  
  if (SPIFFS.exists("/data.dat") && WiFi.status() == WL_CONNECTED){
    DataPoint d;
    memset(&d, 0, sizeof(d));
    size_t readPos = 0;
    bool failedWrite = false;
    File f = SPIFFS.open("/data.dat", "r");
    size_t fileSize = f.size();
    f.close();
    while ((readPos = readDataPoint(&d, readPos)) != 0 && !failedWrite) {
      
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
        yield();
        delay(50);
      } while(tries++ < 3 && !postDataPointToInfluxDB(&d));
      if (tries >= 3) failedWrite = true;
    }
    if(!failedWrite){
      Serial.printf("fully uploaded %db. Removing /data.dat\n", fileSize);
      SPIFFS.remove("/data.dat");
    }
  }

  unsigned long delta = micros() - startMicros;
  if(delta >= sleepMicros) delta = sleepMicros;
  tm = time(nullptr);
  // before time is jan 1 2000
  if (tm > 946684800 && tm < 3559717660){ // very high number to prevent some weirdness
    time_t tr;
    if (readRTCMem(&tr)) Serial.printf("current rtc-time %d\n", tr);
    writeRTCData(tm, ((uint64_t)(sleepMicros-delta)));
  }


  #ifdef ESP_DEEPSLEEP
    Serial.printf("DEEPSLEEP for %.2fs\n", (sleepMicros-delta)/1000000.0f);
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
