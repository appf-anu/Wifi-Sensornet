#include <DHT.h>              //https://github.com/adafruit/DHT-sensor-library
#include <Adafruit_Sensor.h>  //https://github.com/adafruit/DHT-sensor-library
#define MAX_TRIES_DHT22 5
#define DHT22_MAX_TEMP 125
#define DHT22_MIN_TEMP -40
#define DHT22_MAX_HUM 100
#define DHT22_MIN_HUM 0

bool isValidDHT22(float temperature, float humidity){
  /**
   * Checks to see if values recieved from a DHT22 are valid / within the bounds set by 
   * the defines DHT22_MAX_TEMP DHT22_MIN_TEMP DHT22_MAX_HUM DHT22_MIN_HUM 
   */
  if (isnan(temperature) || isnan(humidity)) return false;
  if (temperature < DHT22_MIN_TEMP || temperature > DHT22_MAX_TEMP) return false;
  if (humidity < DHT22_MIN_HUM || humidity > DHT22_MAX_HUM) return false;
  return true;
}

DHT dht(ONE_WIRE_PIN, DHT22);
bool readDHT(){
  /**
   * reads values from a DHT22 sensor and adds the data to the sender.
   * should have the ONE_WIRE_PIN define var set.
   * 
   * @return bool whether the sensor was successfully read and datapoints added to sender
   */
  dht.begin();
  Serial.println("read DHT22");

  float hum = dht.readHumidity();
  float temp = dht.readTemperature();
  size_t tries = 0;
  time_t t;
  do {
    t = time(nullptr);
    hum = dht.readHumidity();
    temp = dht.readTemperature();
    delay(100);
  } while (tries++ < MAX_TRIES_DHT22 && !isValidDHT22(temp, hum));
  if (tries >= MAX_TRIES_DHT22) return false;
  DataSender<DataPoint> sender(3);
  DataPoint airTemperature = createDataPoint(FLOAT, "airTemperature", "dht22", temp, t);
  sender.push_back(airTemperature);
  DataPoint airRelativeHumidity = createDataPoint(FLOAT, "airRelativeHumidity", "dht22", hum, t);
  sender.push_back(airRelativeHumidity);
  sender.push_environment_data(temp, hum);
  return true;
}