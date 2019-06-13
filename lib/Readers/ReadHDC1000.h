#include <Adafruit_HDC1000.h>  // https://github.com/jshnaidman/HDC

#define MAX_TRIES_HDC 5
#define HDC_MAX_TEMP 125
#define HDC_MIN_TEMP -40
#define HDC_MAX_HUM 100
#define HDC_MIN_HUM 0

bool isValidHDC(float temperature, float humidity){
  if (isnan(temperature) || isnan(humidity)) return false;
  if (temperature < HDC_MIN_TEMP || temperature > HDC_MAX_TEMP) return false;
  if (humidity < HDC_MIN_HUM || humidity > HDC_MAX_HUM) return false;
  return true;
}

Adafruit_HDC1000 hdc;
bool readHDC(byte addr){
  
  hdc.begin(addr);
  Serial.println("Read From HDC");
  
  float hum = hdc.readHumidity();
  float temp = hdc.readTemperature();
  size_t tries = 0;
  time_t t;
  do {
    t = time(nullptr);
    hum = hdc.readHumidity();
    temp = hdc.readTemperature();
    delay(100);
  } while (tries++ < MAX_TRIES_HDC && !isValidHDC(temp, hum));
  if (tries >= MAX_TRIES_HDC) return false;
  
  DataSender<DataPoint> sender(3);
  DataPoint airTemperature = createDataPoint(FLOAT, "airTemperature", "hdc", temp, t);
  sender.push_back(airTemperature);
  DataPoint airRelativeHumidity = createDataPoint(FLOAT, "airRelativeHumidity", "hdc", hum, t);
  sender.push_back(airRelativeHumidity);
  sender.push_environment_data(temp, hum);
  return true;
}