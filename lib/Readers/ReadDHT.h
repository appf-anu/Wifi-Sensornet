#include <DHT.h>              //https://github.com/adafruit/DHT-sensor-library
#include <Adafruit_Sensor.h>  //https://github.com/adafruit/DHT-sensor-library
#define MAX_TRIES 5
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
  unsigned long int t = timeClient.getEpochTime();
  dht.begin();
  Serial.println("Read From DHT22");

  float hum = dht.readHumidity();
  float temp = dht.readTemperature();
  size_t tries = 0;
  do {
    t = timeClient.getEpochTime();
      
    hum = dht.readHumidity();
    temp = dht.readTemperature();
    delay(100);
  } while (tries++ < MAX_TRIES && !isValidDHT22(temp, hum));
  if (tries >= MAX_TRIES) return false;
  
  DataPoint dps[2];
  
  
  dps[0] = createDataPoint(FLOAT, "airTemperature", "dht22", temp, t);
  dps[1] = createDataPoint(FLOAT, "airRelativeHumidity", "dht22", hum, t);
  bulkOutputDataPoints(dps, 2, "dht22", t);
  
  size_t n = createEnvironmentData("dht22", t, temp, hum);
  bulkOutputDataPoints(env, n, "dht22", t);
  return true;
}