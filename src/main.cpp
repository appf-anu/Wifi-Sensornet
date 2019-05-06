#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <SPI.h>
#include <ConfigManager.h>

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <ESP8266HTTPClient.h>
//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <WiFiUdp.h>
#include <Ticker.h>
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <NTPClient.h>
#include <I2CSoilMoistureSensor.h>
#include <EnvironmentCalculations.h>
#include <BME280I2C.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <ESP8266mDNS.h>
#include <fwUpdater.h>

// DEFINES
#define DEBUG false
#define UPDATE_HOURS 2

#define DHTPIN D5
#define SDA D2
#define SCL D1

#define ALTITUDECONSTANT 577.0f


ADC_MODE(ADC_VCC);
I2CSoilMoistureSensor chirpSensor;
DHT dht(DHTPIN, DHT22);


// You can specify the time server pool and the offset (in seconds, can be
// changed later with setTimeOffset() ). Additionaly you can specify the
// update interval (in milliseconds, can be changed using setUpdateInterval() ).
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "au.pool.ntp.org", 0);
struct Config cfg;

// this include MUST BE AFTER TIMECLIENT AND CFG!!!
#include <DataManager.h>

// some global(ish) variables
static uint32_t tick = 0;
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}


unsigned int sleepMinutes;
unsigned long sleepMicros;
unsigned int flashButtonCounter = 0;

void flashLed(){
  int state = digitalRead(LED_BUILTIN);  // get the current state of GPIO1 pin
  digitalWrite(LED_BUILTIN, !state);     // set pin to the opposite state
}
 
Ticker ticker;

void resetConfiguration(){
  flashButtonCounter++;
  
  // need to mash the flash button
  if (flashButtonCounter < 5) {
    Serial.printf("press the flash button %d more times to ota next interval...\n", 5-flashButtonCounter);
    return;
  }
  
  ticker.attach(1.0/(float)flashButtonCounter, flashLed);
  tick = (uint32_t)((UPDATE_HOURS*60)/sleepMinutes) - 1;
  
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
  Serial.println("Resetting configuration....");
  ticker.attach(1.0/(float)flashButtonCounter, flashLed);
  // SPIFFS.remove("/config.json");
  
  WiFiManager wifiManager;
  // reset settings - for testing
  wifiManager.resetSettings();
  WiFi.disconnect();
  Serial.println("Config reset, rebooting...");
  ESP.reset();
}

void setup() {

  Serial.println("mounting FS...");
  while (!SPIFFS.begin()) {
    Serial.println("failed to mount FS");
    delay(100);
  }
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(0, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(0), resetConfiguration, FALLING);

  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.println();

  //clean FS, for testing
  //SPIFFS.format();
  //read configuration from FS json
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
  
  if (!wifiManager.autoConnect("Wifi-Sensornet")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

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
#if !DEBUG
  checkForUpdates();
#else
  Serial.printf("Sketch md5: %s\n", ESP.getSketchMD5().c_str());
#endif

}

void outputEnvironmentVars(unsigned long int t, float temp, float hum, float pres){
  EnvironmentCalculations::TempUnit     envTempUnit =  EnvironmentCalculations::TempUnit_Celsius;
  // abshum in grams/m³
  outputPoint("airAbsoluteHumidity", 
    EnvironmentCalculations::AbsoluteHumidity(temp, hum, envTempUnit),
    t);

  outputPoint(INT, "airHeatIndex", 
    EnvironmentCalculations::HeatIndex(temp, hum, envTempUnit), 
    t);
  outputPoint("airDewPoint", 
    EnvironmentCalculations::DewPoint(temp, hum, envTempUnit), 
    t);
  // saturated vapor pressure (es)
  float saturatedVapourPressure = 0.6108 * exp(17.27*temp/(temp+237.3));
  outputPoint("airSaturatedVaporPressure", saturatedVapourPressure, t);
  // actual vapor pressure (ea)
  float actualVapourPressure = hum / 100 * saturatedVapourPressure;
  outputPoint("airActualVaporPressure", saturatedVapourPressure, t);

  // this equation returns a negative value (in kPa), which while technically correct,
  // is invalid in this case because we are talking about a deficit.
  double vapourPressureDeficit = (actualVapourPressure - saturatedVapourPressure) * -1;
  outputPoint("airVapourPressureDeficit", vapourPressureDeficit, t);

  // mixing ratio
  float mixingRatio = 621.97 * actualVapourPressure / ((pres/10) - actualVapourPressure);
  outputPoint("airMixingRatio", mixingRatio, t);
  // saturated mixing ratio
  float saturatedMixingRatio = 621.97 * saturatedVapourPressure / ((pres/10) - saturatedVapourPressure);
  outputPoint("airSaturatedMixingRatio", saturatedMixingRatio, t);
}

void outputEnvironmentVars(unsigned long int t, float temp, float hum){
  EnvironmentCalculations::TempUnit     envTempUnit =  EnvironmentCalculations::TempUnit_Celsius;
  // abshum in grams/m³
  outputPoint("airAbsoluteHumidity", 
    EnvironmentCalculations::AbsoluteHumidity(temp, hum, envTempUnit),
    t);
  outputPoint(INT, "airHeatIndex", 
    EnvironmentCalculations::HeatIndex(temp, hum, envTempUnit), 
    t);
  outputPoint("airDewPoint", 
    EnvironmentCalculations::DewPoint(temp, hum, envTempUnit), 
    t);
  // saturated vapor pressure (es)
  float saturatedVapourPressure = 0.6108 * exp(17.27*temp/(temp+237.3));
  outputPoint("airSaturatedVaporPressure", saturatedVapourPressure, t);
  // actual vapor pressure (ea)
  float actualVapourPressure = hum / 100 * saturatedVapourPressure;
  outputPoint("airActualVaporPressure", saturatedVapourPressure, t);
  // this equation returns a negative value (in kPa), which while technically correct,
  // is invalid in this case because we are talking about a deficit.
  double vapourPressureDeficit = (actualVapourPressure - saturatedVapourPressure) * -1;
  outputPoint("airVapourPressureDeficit", vapourPressureDeficit, t);
}

void readBme280(int address){
  unsigned long int t;
  unsigned int tries = 0;
  BME280I2C::Settings settings(
   BME280::OSR_X1,
   BME280::OSR_X1,
   BME280::OSR_X1,
   BME280::Mode_Forced,
   BME280::StandbyTime_1000ms,
   BME280::Filter_16,
   BME280::SpiEnable_False,
   (address == 0x76 ? BME280I2C::I2CAddr_0x76:BME280I2C::I2CAddr_0x77)
  );
  BME280I2C bme(settings);
  
  while (!bme.begin()){
    if (tries >= 10) {
      Serial.println("Tried too many times to communicate with bme280");
      return;
    }
    tries++;
    delay(300);
  }

  float temp = 0;
  float hum  = 0;
  float pres = 0;
  BME280::TempUnit tempUnit = BME280::TempUnit_Celsius;
  BME280::PresUnit presUnit = BME280::PresUnit_hPa;
  EnvironmentCalculations::TempUnit     envTempUnit =  EnvironmentCalculations::TempUnit_Celsius;
  EnvironmentCalculations::AltitudeUnit envAltUnit  =  EnvironmentCalculations::AltitudeUnit_Meters;
  if (bme.chipModel() ==  BME280::ChipModel_BME280){
    t = timeClient.getEpochTime();
    bme.read(pres, temp, hum, tempUnit, presUnit);
    
    outputPoint("airPressure", pres, t);
    outputPoint("airTemperature", temp, t);
    outputPoint("airRelativeHumidity", hum, t);
    outputPoint("airEquivalentSeaLevelPressure", 
      EnvironmentCalculations::EquivalentSeaLevelPressure(ALTITUDECONSTANT, temp, pres, envAltUnit, envTempUnit),
      t);
    outputEnvironmentVars(t, temp, hum, pres);
  }
  if (bme.chipModel() == BME280::ChipModel_BMP280){
    t = timeClient.getEpochTime();
    bme.read(pres, temp, hum, tempUnit, presUnit);
    
    outputPoint("airPressure", pres, t);
    outputPoint("airTemperature", temp, t);
    outputPoint("airEquivalentSeaLevelPressure", 
      EnvironmentCalculations::EquivalentSeaLevelPressure(ALTITUDECONSTANT, temp, pres, envAltUnit, envTempUnit),
      t);
  }
}

void readDHT(){
  unsigned long int t = timeClient.getEpochTime();
  dht.begin();
  
  float hum = dht.readHumidity();
  float temp = dht.readTemperature();
  
  if (isnan(temp) || isnan(hum)){
    Serial.println("No DHT");
    return;
  }
  outputPoint("airTemperature", temp, t);
  outputPoint("airRelativeHumidity", hum, t);
  outputEnvironmentVars(t, temp, hum);
}

void readChirp(){
  // chirp soil moisture sensor is address 0x20
  unsigned long int t = timeClient.getEpochTime();
  chirpSensor.begin(true); // wait needs 1s for startup
  while (chirpSensor.isBusy()) delay(50);
  unsigned int soilCapacitance = chirpSensor.getCapacitance();
  outputPoint(INT, "soilCapacitance", soilCapacitance, t);
  float soilTemperature = chirpSensor.getTemperature()/(float)10; // temperature is in multiple of 10
  outputPoint("soilTemperature", soilTemperature, t);
  chirpSensor.sleep();
}

void readSys(){
  outputPoint(INT, "espVcc", ESP.getVcc());
  outputPoint(INT, "espFreeHeap", ESP.getFreeHeap());
  outputPoint(INT, "espHeapFragmentation", ESP.getHeapFragmentation());
  float until = (int)(UPDATE_HOURS*60)-(tick-1) % (int)(UPDATE_HOURS*60);
  outputPoint(INT, "secondsUntilNextUpdate", (int)until*60);
}

void loop() {
  // run update on second loop (tick+1)
  if ((tick+1) % (unsigned int)((UPDATE_HOURS*60)/sleepMinutes) == 0){
    checkForUpdates();
  }else{
    Serial.printf("Not time for update %d/%d\n", tick, (unsigned int)((UPDATE_HOURS*60)/sleepMinutes));
  }
  tick ++;
  
  unsigned long startMicros = micros();
  for (int address = 1; address < 127; address++){
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0){
      Serial.printf("Found I2C device at address 0x%02X\n", address);
      if (address == 0x20){
        readChirp();
      }
      if (address == 0x76 || address == 0x77) {
        readBme280(address);
      }
    }
  }
  
  readDHT();
  
  readSys();
  
  if (SPIFFS.exists("/data.dat")){
    File f = SPIFFS.open("/data.dat", "r");
    
    outputPoint(INT, "dataFileSize", f.size());
    f.close();
    DataPoint dr;
    memset(&dr, 0, sizeof(dr));
    size_t readPos = 0;
    bool failedWrite = false;
    while ((readPos = readDataPoint(&dr, readPos)) != 0) {
        Serial.printf("%ld %s %.01f\t[]->\t", dr.time, dr.name, dr.value);
        for (size_t tries = 0; tries < 3; tries++){
          if(postDataPointToInfluxDB(&dr)) break;
          failedWrite = true;
          delay(100);
        }
        
    }
    if(!failedWrite) SPIFFS.remove("/data.dat");
  }
  unsigned long delta = micros() - startMicros;
  #ifdef ESP_DEEPSLEEP
    ESP.deepSleep(sleepMicros-delta);
  #else
    unsigned long sleepTotal = (sleepMicros-delta)/1000;
    Serial.printf("Delay for %.3fs\n", sleepTotal/(float)1000);
    delay(sleepTotal);  
  #endif
}