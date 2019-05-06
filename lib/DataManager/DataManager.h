#include <FS.h>
#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>

typedef enum {
    INT,
    FLOAT
} TYPE;

struct DataPoint
{
    char name[32];
    unsigned long time;
    float value;
    TYPE type;
};

int writeDataPoint(DataPoint *d){
    Serial.printf("Writing data point %lu %s %.2f\n", d->time, d->name, d->value);
    File f = SPIFFS.open("/data.dat", "a+");
    f.seek(f.size(), SeekSet);
    f.write((uint8_t *)d, sizeof(*d));
    f.close();
    return 0;
}

int readDataPoint(DataPoint *d, size_t seekNum){
    if (!SPIFFS.exists("/data.dat")) {
        Serial.println("Data file does not exist");
        return 0;
    }
    //file exists, reading and loading
    File f = SPIFFS.open("/data.dat", "r");
    if (!f) {
        Serial.println("Failed to open data file");
        return 0;
    }
    if (seekNum >= f.size()) return 0;
    f.seek(seekNum);
    memset(d, 0, sizeof(*d));
    f.read((uint8_t *)d, sizeof(*d));
    return seekNum + sizeof(*d);
}


int postDataPointToInfluxDB(DataPoint *d){
  char url[256];
  sprintf(url, "http://%s:%s/write?db=%s&precision=s&u=%s&p=%s", 
    cfg.influxdb_server, cfg.influxdb_port,
    cfg.influxdb_db, 
    cfg.influxdb_user, 
    cfg.influxdb_password);
  char metric[256];
  int chipId = ESP.getChipId();
  String sketchmd5 = ESP.getSketchMD5();
  
  switch (d->type){
    case INT:
      sprintf(metric, "sensornode,chipid=%d,sketchmd5=%s,location=%s %s=%di %lu",
        chipId, sketchmd5.c_str(),
        cfg.location, // change these out later to come from the config.
        d->name, (int)d->value,
        d->time);
      break;
    case FLOAT:
      sprintf(metric, "sensornode,chipid=%d,sketchmd5=%s,location=%s %s=%.5f %lu",
        chipId, sketchmd5.c_str(),
        cfg.location, // change these out later to come from the config.
        d->name, d->value,
        d->time);
      break;
  }
  Serial.println(metric);

#if DEBUG 
  return 1;
#endif

  // http request
  WiFiClient wifi;
  HTTPClient http;
  http.setTimeout(10000);
  http.begin(wifi, url);
  http.addHeader("Content-Type", "text/plain");
  int httpCode = http.POST(metric);
  http.end();
  if (!(httpCode == HTTP_CODE_NO_CONTENT || httpCode == HTTP_CODE_OK))
    Serial.printf("Executed POST to %s returned %d\n", url, httpCode);
  return httpCode == HTTP_CODE_NO_CONTENT || httpCode == HTTP_CODE_OK;
}


void outputPoint(TYPE dtype, const char name[32], float value, unsigned long int t){
  DataPoint d;
  memset(&d, 0, sizeof(d));
  d.time = t;
  d.type = dtype;
  strcpy(d.name, name);
  d.value = value;
  if (WiFi.status() != WL_CONNECTED) {
    writeDataPoint(&d);
    return;
  }
  size_t tries;
  for (tries = 0; tries < 3; tries++ ){
    if (postDataPointToInfluxDB(&d)){
      delay(100); // sleep a little bit so as not to hammer the server.
      return;
    } 
  }
  writeDataPoint(&d);
}

void outputPoint(TYPE dtype, const char name[32], float value){
  outputPoint(dtype, name, value, timeClient.getEpochTime());
}
void outputPoint(const char name[32], float value, unsigned long int t){
  outputPoint(FLOAT, name, value, t);
}
void outputPoint(const char name[32], float value){
  outputPoint(FLOAT, name, value, timeClient.getEpochTime());
}
