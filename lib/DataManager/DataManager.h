#include <FS.h>
#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <EnvironmentCalculations.h>  //https://github.com/finitespace/BME280

typedef enum {
    INT,
    FLOAT
} TYPE;

struct DataPoint
{
    char name[32];
    char sensorType[8];
    unsigned long time;
    float value;
    TYPE type;
};

int writeDataPoint(DataPoint *d){
    File f = SPIFFS.open("/data.dat", "a+");
    f.seek(f.size(), SeekSet);
    f.write((uint8_t *)d, sizeof(*d));
    f.close();
    switch (d->type)
    {
      case INT:
        Serial.printf("[]<- %lu %s %d\n", d->time, d->name, (int)d->value);
        break;
      case FLOAT:
        Serial.printf("[]<- %lu %s %.2f\n", d->time, d->name, d->value);
        break;
    } 
    delay(20);
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

DataPoint createDataPoint(TYPE dtype, const char name[32], const char *sensorType, float value, unsigned long int t){
  DataPoint d;
  memset(&d, 0, sizeof(d));
  d.time = t;
  d.type = dtype;
  strncpy(d.sensorType, sensorType, 8);
  strcpy(d.name, name);
  d.value = value;
  return d;
}

DataPoint createDataPoint(TYPE dtype, const char name[32], const char *sensorType, float value){
  return createDataPoint(dtype, name, sensorType, value, timeClient.getEpochTime());
}

int postBulkDataPointsToInfluxdb(DataPoint *d, size_t num, const char *sensorType, unsigned long t){
  char url[256];
  memset(url, 0, sizeof(url));
  sprintf(url, "http://%s:%s/write?db=%s&precision=s&u=%s&p=%s", 
    cfg.influxdb_server, cfg.influxdb_port,
    cfg.influxdb_db, 
    cfg.influxdb_user, 
    cfg.influxdb_password);
  
  char metric[1024];
  memset(metric, 0, sizeof(metric));
  int chipId = ESP.getChipId();
  String sketchmd5 = ESP.getSketchMD5();
  
  strcpy(metric, "sensornode");
  sprintf(metric, "%s,chipid=%d,sketchmd5=%s,location=%s",
          metric, 
          chipId, sketchmd5.c_str(), cfg.location);
  
  if (strcmp(sensorType, "") != 0){
    sprintf(metric, "%s,sensorType=%s", metric, sensorType);
  }
  strcat(metric, " ");
  for (size_t x =0; x < num; x++){
    switch ((d+x)->type){
      case INT:
        sprintf(metric, "%s%s=%di", metric, (d+x)->name, (int)(d+x)->value);
      break;
      case FLOAT:
        sprintf(metric, "%s%s=%.5f", metric, (d+x)->name, (float)(d+x)->value);
    }
    if (x != num-1) strcat(metric, ",");
  }
  sprintf(metric, "%s %lu", metric, t);
  

#if DEBUG 
  Serial.println(metric);
  return 1;
#endif
  
  // http request
  WiFiClient wifi;
  HTTPClient http;
  http.setTimeout(10000);
  http.begin(wifi, url);
  http.addHeader("Content-Type", "text/plain");
  int httpCode = http.POST(metric);
  String payload = http.getString();
  
  Serial.printf("posted %s: %db to server got %d\n", sensorType, strlen(metric), httpCode);
  if (!(httpCode == HTTP_CODE_NO_CONTENT || httpCode == HTTP_CODE_OK))
    Serial.printf("POST to %s returned %d: %s\n", url, httpCode, payload.c_str());

  http.end();
  return httpCode == HTTP_CODE_NO_CONTENT || httpCode == HTTP_CODE_OK;
}

void bulkOutputDataPoints(DataPoint *d, size_t num, const char *sensorType, unsigned long t){
  if (WiFi.status() != WL_CONNECTED) {
    for (size_t x = 0; x < num; x++) writeDataPoint(d+x);
    return;
  }

  for (size_t tries = 0; tries < 3; tries++ ){
    if (postBulkDataPointsToInfluxdb(d, num, sensorType, t)){
      delay(100); // sleep a little bit so as not to hammer the server.
      return;
    }
  }

  for (size_t x = 0; x < num; x++) writeDataPoint(d+x);
}


int postDataPointToInfluxDB(DataPoint *d){
  char url[256];
  sprintf(url, "http://%s:%s/write?db=%s&precision=s&u=%s&p=%s", 
    cfg.influxdb_server, cfg.influxdb_port,
    cfg.influxdb_db, 
    cfg.influxdb_user, 
    cfg.influxdb_password);
  char metric[512];
  // memset(metric, 0, sizeof(metric));
  int chipId = ESP.getChipId();
  String sketchmd5 = ESP.getSketchMD5();
  
  strcpy(metric, "sensornode");
  sprintf(metric, "%s,chipid=%d,sketchmd5=%s,location=%s",
          metric, 
          chipId, sketchmd5.c_str(), cfg.location);
  
  if (strcmp(d->sensorType, "") != 0){
    sprintf(metric, "%s,sensorType=%s", metric, d->sensorType);
  }
  switch (d->type){
    case INT:
      sprintf(metric, "%s %s=%di %lu", metric, d->name, (int)d->value, d->time);
    break;
    case FLOAT:
      sprintf(metric, "%s %s=%.5f %lu", metric, d->name, (float)d->value, d->time);
  }
  

#if DEBUG
  Serial.println(metric);
  return 1;
#endif

  // http request
  WiFiClient wifi;
  HTTPClient http;
  http.setTimeout(10000);
  http.begin(wifi, url);
  http.addHeader("Content-Type", "text/plain");
  int httpCode = http.POST(metric);
  String payload = http.getString();
  
  Serial.printf("posted %s: %db to server got %d\n", d->sensorType, strlen(metric), httpCode);
  if (!(httpCode == HTTP_CODE_NO_CONTENT || httpCode == HTTP_CODE_OK))
    Serial.printf("POST to %s returned %d: %s\n", url, httpCode, payload.c_str());
    
  http.end();
  return httpCode == HTTP_CODE_NO_CONTENT || httpCode == HTTP_CODE_OK;
}

void outputPoint(TYPE dtype, const char name[32], const char *sensorType, float value, unsigned long int t){
  DataPoint d;
  memset(&d, 0, sizeof(d));
  d.time = t;
  d.type = dtype;
  strncpy(d.sensorType, sensorType, 8);
  strcpy(d.name, name);
  d.value = value;
  if (WiFi.status() != WL_CONNECTED) {
    writeDataPoint(&d);
    return;
  }
  for (size_t tries = 0; tries < 3; tries++ ){
    if (postDataPointToInfluxDB(&d)){
      delay(100); // sleep a little bit so as not to hammer the server.
      return;
    } 
  }
  writeDataPoint(&d);
}

size_t createEnvironmentData(DataPoint dps[8], const char *sensorType, unsigned long int t, float temp, float hum, float pres){
  EnvironmentCalculations::TempUnit     envTempUnit =  EnvironmentCalculations::TempUnit_Celsius;
  // abshum in grams/m³

  float airAbsoluteHumidity = EnvironmentCalculations::AbsoluteHumidity(temp, hum, envTempUnit);
  dps[0] = createDataPoint(FLOAT, "airAbsoluteHumidity", sensorType, airAbsoluteHumidity, t);

  unsigned int airHeatIndex = EnvironmentCalculations::HeatIndex(temp, hum, envTempUnit);

  dps[1] = createDataPoint(INT, "airHeatIndex", sensorType, airHeatIndex, t);
  float airDewPoint = EnvironmentCalculations::DewPoint(temp, hum, envTempUnit);
  dps[2] = createDataPoint(FLOAT, "airDewPoint", sensorType, airDewPoint, t);
  // saturated vapor pressure (es)
  float saturatedVapourPressure = 0.6108 * exp(17.27*temp/(temp+237.3));
  dps[3] = createDataPoint(FLOAT, "airSaturatedVaporPressure", sensorType, saturatedVapourPressure, t);
  // actual vapor pressure (ea)
  float actualVapourPressure = hum / 100 * saturatedVapourPressure;
  dps[4] = createDataPoint(FLOAT, "airActualVaporPressure", sensorType, saturatedVapourPressure, t);

  // this equation returns a negative value (in kPa), which while technically correct,
  // is invalid in this case because we are talking about a deficit.
  double vapourPressureDeficit = (actualVapourPressure - saturatedVapourPressure) * -1;
  dps[5] = createDataPoint(FLOAT, "airVapourPressureDeficit", sensorType, vapourPressureDeficit, t);

  // mixing ratio
  float mixingRatio = 621.97 * actualVapourPressure / ((pres/10) - actualVapourPressure);
  dps[6] = createDataPoint(FLOAT, "airMixingRatio", sensorType, mixingRatio, t);
  // saturated mixing ratio
  float saturatedMixingRatio = 621.97 * saturatedVapourPressure / ((pres/10) - saturatedVapourPressure);
  dps[7] = createDataPoint(FLOAT, "airSaturatedMixingRatio", sensorType, saturatedMixingRatio, t);
  return 8;
}

size_t createEnvironmentData(DataPoint dps[6], const char *sensorType, unsigned long int t, float temp, float hum){
  
  EnvironmentCalculations::TempUnit     envTempUnit =  EnvironmentCalculations::TempUnit_Celsius;
  // abshum in grams/m³
  float airAbsoluteHumidity = EnvironmentCalculations::AbsoluteHumidity(temp, hum, envTempUnit);
  dps[0] = createDataPoint(FLOAT, "airAbsoluteHumidity", sensorType, airAbsoluteHumidity, t);
  
  unsigned int airHeatIndex = EnvironmentCalculations::HeatIndex(temp, hum, envTempUnit);
  dps[1] = createDataPoint(INT, "airHeatIndex", sensorType, airHeatIndex, t);
  float airDewPoint = EnvironmentCalculations::DewPoint(temp, hum, envTempUnit);
  dps[2] = createDataPoint(FLOAT, "airDewPoint", sensorType, airDewPoint, t);
  // saturated vapor pressure (es)
  float saturatedVapourPressure = 0.6108 * exp(17.27*temp/(temp+237.3));
  dps[3] = createDataPoint(FLOAT, "airSaturatedVaporPressure", sensorType, saturatedVapourPressure, t);
  // actual vapor pressure (ea)
  float actualVapourPressure = hum / 100 * saturatedVapourPressure;
  dps[4] = createDataPoint(FLOAT, "airActualVaporPressure", sensorType, saturatedVapourPressure, t);
  // this equation returns a negative value (in kPa), which while technically correct,
  // is invalid in this case because we are talking about a deficit.
  double vapourPressureDeficit = (actualVapourPressure - saturatedVapourPressure) * -1;
  dps[5] = createDataPoint(FLOAT, "airVapourPressureDeficit", sensorType, vapourPressureDeficit, t);
  return 6;
}