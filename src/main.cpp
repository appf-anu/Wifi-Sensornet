#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <Arduino.h>
#include <ConfigManager.h>
#include <DataManager.h>
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <ESP8266HTTPClient.h>
//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <WiFiUdp.h>
#include <NTPClient.h>

#include <I2CSoilMoistureSensor.h>

#include <Wire.h>
ADC_MODE(ADC_VCC);

I2CSoilMoistureSensor chirpSensor;
WiFiUDP ntpUDP;

// You can specify the time server pool and the offset (in seconds, can be
// changed later with setTimeOffset() ). Additionaly you can specify the
// update interval (in milliseconds, can be changed using setUpdateInterval() ).
NTPClient timeClient(ntpUDP, "au.pool.ntp.org", 0);

bool shouldSaveConfig = false;
//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

struct Config cfg;

int sleepMinutes;
unsigned long sleepMicros;

void resetConfiguration(){
  Serial.println("Resetting configuration....");
  SPIFFS.remove("/config.json");
  WiFiManager wifiManager;
  // reset settings - for testing
  wifiManager.resetSettings();
  Serial.println("Config reset, rebooting...");
  ESP.reset();
}

void setup() {
  while (!SPIFFS.begin()) {
    Serial.println("failed to mount FS");
    delay(100);
  }
  pinMode(0, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(0), resetConfiguration, FALLING);

  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.println();

  //clean FS, for testing
  //SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");
  loadConfig(&cfg);

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_influxdb_server("server", "influxdb server", cfg.influxdb_server, 64);
  WiFiManagerParameter custom_influxdb_port("port", "influxdb port", cfg.influxdb_port, 6);
  WiFiManagerParameter custom_influxdb_db("database", "influxdb db", cfg.influxdb_port, 16);
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

  timeClient.begin();
  Serial.println("Updating time...");
  timeClient.forceUpdate();
  Wire.setClockStretchLimit(2500);

  // i2c:   sda,scl
  // nodemcu D6,D7
  // esp8266 12,13
  Wire.begin(12,13);
  sleepMinutes = atoi(cfg.interval);
  sleepMicros  = sleepMinutes*6e+7;

}

int postDataPointToInfluxDB(DataPoint *d){
  char url[128];
  sprintf(url, "http://%s:%s/write?db=%s&precision=s&u=%s&p=%s", 
    cfg.influxdb_server, cfg.influxdb_port,
    cfg.influxdb_db, cfg.influxdb_user, cfg.influxdb_password);
  char metric[128];
  int chipId = ESP.getChipId();

  sprintf(metric, "solarnode,chipid=%d,location=%s %s=%.5f %lu",
    chipId, cfg.location, // change these out later to come from the config.
    d->name, d->value,
    d->time);

  // http request
  WiFiClient wifi;
  HTTPClient http;
  http.setTimeout(10000);
  http.begin(wifi, url);
  int httpCode = http.POST(metric);
  http.end();
  
  if (httpCode == HTTP_CODE_NO_CONTENT || httpCode == HTTP_CODE_OK) {
    return 0;
  }
  return 1;
}

void outputPoint(const char name[32], float value, unsigned long int t){
  DataPoint d;
  memset(&d, 0, sizeof(d));
  d.time = t;
  strcpy(d.name, name);
  d.value = value;
  if (WiFi.status() != WL_CONNECTED) {
    writeDataPoint(&d);
    return;
  }
  size_t tries;
  for (tries = 0; tries < 5; tries++ ){
    if (postDataPointToInfluxDB(&d)) break;
  }
  if (tries == 4){
    writeDataPoint(&d);
  } 
}

void outputPoint(const char name[32], float value){
  outputPoint(name, value, timeClient.getEpochTime());
}

int cntr = 0;
void loop() {
  unsigned long startMicros = micros();
  for (int address = 1; address < 127; address++){
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0){
      if (address == 0x20){
        // chirp soil moisture sensor is address 0x20
        unsigned long int t = timeClient.getEpochTime();
        chirpSensor.begin(true); // wait needs 1s for startup
        while (chirpSensor.isBusy()) delay(50);
        float soilCapacitance = (float)chirpSensor.getCapacitance();
        outputPoint("soilCapacitance", soilCapacitance, t);

        float soilTemperature = chirpSensor.getTemperature()/(float)10; // temperature is in multiple of 10
        outputPoint("soilTemperature", soilTemperature, t);
        chirpSensor.sleep();
      }
      // Serial.printf("Found I2C device at address 0x%02X\n", address);
    }
  }

  outputPoint("espvcc", (float)ESP.getVcc());

  if (SPIFFS.exists("/data.dat")){
    File f = SPIFFS.open("/data.dat", "r");
    outputPoint("dataFileSize", float(f.size()));
    f.close();
    DataPoint dr;
    memset(&dr, 0, sizeof(dr));
    size_t readPos = 0;
    
    while ((readPos = readDataPoint(&dr, readPos)) != 0) {
        Serial.printf("%ld %s %.01f\n", dr.time, dr.name, dr.value);
        outputPoint(dr.name, dr.value, dr.time);
    }
    SPIFFS.remove("/data.dat");
  }

  unsigned long delta = micros() - startMicros;
  #ifdef ESP_DEEPSLEEP
    ESP.deepSleep(sleepMicros-delta);
  #else
    delay((sleepMicros-delta)/1000);
  #endif
  
}

