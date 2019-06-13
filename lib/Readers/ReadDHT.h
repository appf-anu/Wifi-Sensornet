#include <DHT.h>              //https://github.com/adafruit/DHT-sensor-library
#include <Adafruit_Sensor.h>  //https://github.com/adafruit/DHT-sensor-library
#define MAX_TRIES_DHT22 5
#define DHT22_MAX_TEMP 125
#define DHT22_MIN_TEMP -40
#define DHT22_MAX_HUM 100
#define DHT22_MIN_HUM 0

bool isValidDHT22(float temperature, float humidity){
  if (isnan(temperature) || isnan(humidity)) return false;
  if (temperature < DHT22_MIN_TEMP || temperature > DHT22_MAX_TEMP) return false;
  if (humidity < DHT22_MIN_HUM || humidity > DHT22_MAX_HUM) return false;
  return true;
}

DHT dht(ONE_WIRE_PIN, DHT22);
bool readDHT(){
  dht.begin();
  Serial.println("Read From DHT22");

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