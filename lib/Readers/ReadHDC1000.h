#include <Adafruit_HDC1000.h>  // https://github.com/jshnaidman/HDC

#define MAX_TRIES 5
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
bool readHDC(unsigned long int t, byte addr){
  
  hdc.begin(addr);
  Serial.println("Read From HDC");
  
  float hum = hdc.readHumidity();
  float temp = hdc.readTemperature();
  size_t tries = 0;
  do {
    hum = hdc.readHumidity();
    temp = hdc.readTemperature();
    delay(100);
  } while (tries++ < MAX_TRIES && !isValidHDC(temp, hum));
  if (tries >= MAX_TRIES) return false;

  if (isnan(temp) || isnan(hum)){
    return false;
  }
  DataPoint dps[2];
  
  
  dps[0] = createDataPoint(FLOAT, "airTemperature", "hdc", temp, t);
  dps[1] = createDataPoint(FLOAT, "airRelativeHumidity", "hdc", hum, t);
  bulkOutputDataPoints(dps, 2, "hdc", t);
  
  size_t n = createEnvironmentData("hdc", t, temp, hum);
  bulkOutputDataPoints(env, n, "hdc", t);
  return true;
}