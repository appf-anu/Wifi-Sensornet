#include <FS.h>
#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <EnvironmentCalculations.h>  //https://github.com/finitespace/BME280

#ifdef ARDUINO_ESP8266_NODEMCU
const char *platform = "nodemcuv2";
#elif defined ARDUINO_ESP8266_ESP12
const char *platform = "esp12e";
#elif defined ARDUINO_ESP8266_ESP01
const char *platform = "esp01";
#else
const char *platform = "unknown";
#endif


const char *urlfmt = "http://%s:%s/write?db=%s&precision=s&u=%s&p=%s";
const char *metricfmt = "%s,platform=%s,chipid=%06X,sketchmd5=%s,location=%s";

typedef enum {
    INT,
    FLOAT,
    DOUBLE,
    BOOL
} TYPE;

struct DataPoint
{
    char name[32];
    char sensorType[8];
    char sensorAddress[16];
    time_t time;
    double value;
    TYPE type;
};

DataPoint env[9];

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
        Serial.printf("[]<- %lu %s %.2f\n", d->time, d->name, (float)d->value);
        break;
      case DOUBLE:
        Serial.printf("[]<- %lu %s %.2f\n", d->time, d->name, (double)d->value);
        break;
      case BOOL:
        Serial.printf("[]<- %lu %s %s\n", d->time, d->name, ((bool)d->value)?"true":"false");
        break;
    } 
    delay(20);
    return 0;
}

int readDataPoint(DataPoint *d, size_t seekNum){
    if (!SPIFFS.exists("/data.dat")) {
        return 0;
    }
    //file exists, reading and loading
    File f = SPIFFS.open("/data.dat", "r");
    if (!f) {
        Serial.println("fail open data");
        return 0;
    }
    if (seekNum >= f.size()) return 0;
    f.seek(seekNum);
    memset(d, 0, sizeof(*d));
    f.read((uint8_t *)d, sizeof(*d));
    return seekNum + sizeof(*d);
}

DataPoint createDataPoint(TYPE dtype, const char name[32], const char sensorType[8], const char sensorAddress[16], double value, time_t t){
  DataPoint d;
  memset(&d, 0, sizeof(d));
  d.time = t;
  d.type = dtype;
  strncpy(d.sensorType, sensorType, 8);
  strcpy(d.name, name);
  d.value = value;
  return d;
}

DataPoint createDataPoint(TYPE dtype, const char name[32], const char sensorType[8], double value, time_t t){
  return createDataPoint(dtype, name, sensorType, "", value, t);
}

WiFiClient wifiClient;
HTTPClient httpClient;

int postMetric(const char *metric, const char sensorType[8]){
  char url[256];
  memset(url, 0, sizeof(url));
  sprintf(url, urlfmt, 
    cfg.influxdb_server, cfg.influxdb_port,
    cfg.influxdb_db, 
    cfg.influxdb_user, 
    cfg.influxdb_password);
    // http request
    httpClient.begin(wifiClient, url);
    int httpCode = httpClient.POST(metric);
    delay(200); // delay a little bit to avoid being got by the wdt

    // Serial.printf("POST %s: %db to server got %d\n", sensorType, strlen(metric), httpCode);
    
    if (!(httpCode == HTTP_CODE_NO_CONTENT || httpCode == HTTP_CODE_OK)){
      String payload = httpClient.getString();
      Serial.printf("\nPOST to %s returned %d: %s\n", url, httpCode, payload.c_str());
      Serial.println(metric);
    }
    httpClient.end();
    delay(100);
    return (httpCode == HTTP_CODE_NO_CONTENT || httpCode == HTTP_CODE_OK);
}


int postDataPointToInfluxDB(DataPoint *d){
  char url[256];
  sprintf(url, urlfmt, 
    cfg.influxdb_server, cfg.influxdb_port,
    cfg.influxdb_db, 
    cfg.influxdb_user, 
    cfg.influxdb_password);
  char metric[512];
  memset(metric, 0, sizeof(metric));
  int chipId = ESP.getChipId();
  String sketchmd5 = ESP.getSketchMD5();
  
  strcpy(metric, "sensornode");
  sprintf(metric, metricfmt,
          metric, platform,
          chipId, sketchmd5.c_str(), cfg.location);
  
  if (strcmp(d->sensorType, "") != 0){
    sprintf(metric, "%s,sensorType=%s", metric, d->sensorType);
  }
  if (strcmp(d->sensorAddress, "") != 0){
    sprintf(metric, "%s,sensorAddr=%s", metric, d->sensorType);
  }
  if (metric[strlen(metric)-2] != ' ') strcat(metric, " ");
  switch (d->type){
    case INT:
      sprintf(metric, "%s%s=%di %lu", metric, d->name, (int)d->value, d->time);
    break;
    case FLOAT:
      sprintf(metric, "%s%s=%.6f %lu", metric, d->name, (float)d->value, d->time);
      break;
    case DOUBLE:
      sprintf(metric, "%s%s=%.6f %lu", metric, d->name, (double)d->value, d->time);
      break;
    case BOOL:
      sprintf(metric, "%s%s=%s", metric, d->name, ((bool)d->value)?"true":"false");
      break;
  }
  return postMetric(metric, d->sensorType);
}


template<class Measurement>
class DataSender {
public:

  DataSender(size_t max_size){
    _write_flash = true;
    _max_size = max_size < 3? 2: max_size;
  }

  DataSender(size_t max_size, bool write_flash){
    _max_size = max_size < 3? 2: max_size;
    _write_flash = write_flash;
  }

  ~DataSender() {
    this->flush();
    Serial.printf("\n%s deconstruct\n", _sensorType);
  }

  bool postBulk(){
    char metric[512];
    memset(metric, 0, sizeof(metric));
    int chipId = ESP.getChipId();
    String sketchmd5 = ESP.getSketchMD5();
    
    strcpy(metric, "sensornode");
    sprintf(metric, metricfmt,
            metric, platform,
            chipId, sketchmd5.c_str(), cfg.location);
    if (strcmp(_sensorType, "") != 0){
      sprintf(metric, "%s,sensorType=%s", metric, _sensorType);
    }
    if (strcmp(_sensorAddr, "") != 0){
      sprintf(metric, "%s,sensorAddr=%s", metric, _sensorAddr);
    }
    if (metric[strlen(metric)-2] != ' ') strcat(metric, " ");
    for (size_t x =0; x < _measurements.size(); x++){
      switch (_measurements[x].type){
        case INT:
          sprintf(metric, "%s%s=%di", metric, _measurements[x].name, (int)_measurements[x].value);
        break;
        case FLOAT:
          sprintf(metric, "%s%s=%.6f", metric, _measurements[x].name, (double)_measurements[x].value);
          break;
        case DOUBLE:
          sprintf(metric, "%s%s=%.6f", metric, _measurements[x].name, (double)_measurements[x].value);
          break;
        case BOOL:
          sprintf(metric, "%s%s=%s", metric, _measurements[x].name, ((bool)_measurements[x].value)?"true":"false");
          break;
      }
      if (x != _measurements.size() - 1) strcat(metric, ",");
      Serial.print(".");
    }
    sprintf(metric, "%s %lu", metric, _t); 
    return postMetric(metric, _sensorType);
  }

  bool flush() {
    if (_measurements.size() == 0){
      Serial.printf("\n%s already flushed\n", _sensorType);
      return true;
    } 
    /// Send my shit
    // Serial.printf("%s flushing", _sensorType);
    // we have already failed
    bool success = false;
    if (WiFi.isConnected()) {
      size_t tries = 0;
      do {
        success = this->postBulk(); // only post if wifi is connected.
      } while (tries++ < 3 && !success);
      
    }
    if (success || !_write_flash) {
      // clear if success
      _measurements.clear(); /// or whatever the method is that empties the array
    } else {
      // count errors
      int v = 0;
      for (size_t x =0; x < _measurements.size(); x++){
        v += writeDataPoint(&_measurements[x]);
        delay(100);
      }
      if (v == 0) _measurements.clear(); // clear if no error writing measurements to flash
      return false; // return false
    }
    return success;
  }

  bool push_back(Measurement &meas) {
    bool success = true;
    bool dirty = false;
    if (meas.time != _t ||
        meas.sensorType != _sensorType || 
        meas.sensorAddress != _sensorAddr){
          dirty = true;
          Serial.print(",");
      }
    
    if (_measurements.size() > _max_size || dirty) {
      success = this->flush();
    }

    _t = meas.time;
    strcpy(_sensorType, meas.sensorType);
    strcpy(_sensorAddr, meas.sensorAddress);
    _measurements.push_back(meas);
    return success;
  }

  void push_environment_data(double temp, double hum){
    EnvironmentCalculations::TempUnit     envTempUnit =  EnvironmentCalculations::TempUnit_Celsius;
    
    // abshum in grams/m³
    double airAbsoluteHumidity = EnvironmentCalculations::AbsoluteHumidity(temp, hum, envTempUnit);
    DataPoint airAbsoluteHumidity_ = createDataPoint(FLOAT, "airAbsoluteHumidity", _sensorType, airAbsoluteHumidity, _t);
    this->push_back(airAbsoluteHumidity_);
    // air heat index
    unsigned int airHeatIndex = EnvironmentCalculations::HeatIndex(temp, hum, envTempUnit);
    DataPoint airHeatIndex_ = createDataPoint(INT, "airHeatIndex", _sensorType, airHeatIndex, _t);
    this->push_back(airHeatIndex_);
    double airDewPoint = EnvironmentCalculations::DewPoint(temp, hum, envTempUnit);
    DataPoint airDewPoint_ = createDataPoint(FLOAT, "airDewPoint", _sensorType, airDewPoint, _t);
    this->push_back(airDewPoint_);
    // saturated vapor pressure (es)
    double saturatedVapourPressure = 0.6108 * exp(17.27*temp/(temp+237.3));
    DataPoint airSaturatedVaporPressure_ = createDataPoint(FLOAT, "airSaturatedVaporPressure", _sensorType, saturatedVapourPressure, _t);
    this->push_back(airSaturatedVaporPressure_);
    // actual vapor pressure (ea)
    double actualVapourPressure = hum / 100 * saturatedVapourPressure;
    DataPoint airActualVaporPressure_ = createDataPoint(FLOAT, "airActualVaporPressure", _sensorType, saturatedVapourPressure, _t);
    this->push_back(airActualVaporPressure_);
    // this equation returns a negative value (in kPa), which is technically correct, but
    // is invalid in this case because we are talking about a deficit.
    double vapourPressureDeficit = (actualVapourPressure - saturatedVapourPressure) * -1;
    DataPoint airVapourPressureDeficit_ = createDataPoint(FLOAT, "airVapourPressureDeficit", _sensorType, vapourPressureDeficit, _t);
    this->push_back(airVapourPressureDeficit_);
  }
  
  void push_environment_data(double temp, double hum, double pres){
    EnvironmentCalculations::TempUnit     envTempUnit =  EnvironmentCalculations::TempUnit_Celsius;
    EnvironmentCalculations::AltitudeUnit envAltUnit  =  EnvironmentCalculations::AltitudeUnit_Meters;
    double eslp = EnvironmentCalculations::EquivalentSeaLevelPressure(ALTITUDECONSTANT, temp, pres, envAltUnit, envTempUnit);
    DataPoint airEquivalentSeaLevelPressure_ = createDataPoint(FLOAT,"airEquivalentSeaLevelPressure", _sensorType, eslp, _t);
    this->push_back(airEquivalentSeaLevelPressure_);
    this->push_environment_data(temp, hum);
  }

protected:
  size_t _max_size;
  unsigned long _t;
  bool _write_flash;
  char _sensorType[8];
  char _sensorAddr[16];
  std::vector<Measurement> _measurements;
};


