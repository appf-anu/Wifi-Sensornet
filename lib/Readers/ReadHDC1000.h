#include <Adafruit_HDC1000.h>  // https://github.com/jshnaidman/HDC

#define MAX_TRIES_HDC 5
#define HDC_MAX_TEMP 125
#define HDC_MIN_TEMP -40
#define HDC_MAX_HUM 100
#define HDC_MIN_HUM 0

bool isValidHDC(float temperature, float humidity){
  /**
   * Checks to see if values recieved from a HDC1000 are valid / within the bounds set by 
   * the defines HDC_MAX_TEMP HDC_MIN_TEMP HDC_MAX_HUM HDC_MIN_HUM 
   */
  if (isnan(temperature) || isnan(humidity)) return false;
  if (temperature < HDC_MIN_TEMP || temperature > HDC_MAX_TEMP) return false;
  if (humidity < HDC_MIN_HUM || humidity > HDC_MAX_HUM) return false;
  return true;
}

Adafruit_HDC1000 hdc;
bool readHDC(byte addr){
  /**
   * reads values from a HDC1000 sensor and adds the data to the sender.
   * 
   * @param byte addr the i2c address of the sensor.
   * @return bool whether the sensor was successfully read and datapoints added to sender.
   */
  
  hdc.begin(addr);
  Serial.println("read HDC");
  
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