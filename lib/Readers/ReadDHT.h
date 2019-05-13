#include <DHT.h>              //https://github.com/adafruit/DHT-sensor-library
#include <Adafruit_Sensor.h>  //https://github.com/adafruit/DHT-sensor-library
#define MAX_TRIES 5
#define MAX_TEMP 200
#define MIN_TEMP -100
#define MAX_HUM 100
#define MIN_HUM 0

DHT dht(DHTPIN, DHT22);
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
  } while (tries++ < MAX_TRIES &&
           ((isnan(temp) || isnan(hum)) ||
            (!(temp < MAX_TEMP && temp > MIN_TEMP) &&
             !(hum  < MAX_HUM  && hum  > MIN_HUM ) )));
  if (tries >= MAX_TRIES) return false;

  if (isnan(temp) || isnan(hum)){
    return false;
  }
  DataPoint dps[2];
  
  
  dps[0] = createDataPoint(FLOAT, "airTemperature", "dht22", temp, t);
  dps[1] = createDataPoint(FLOAT, "airRelativeHumidity", "dht22", hum, t);
  bulkOutputDataPoints(dps, 2, "dht22", t);
  DataPoint env[6];
  size_t n = createEnvironmentData(env, "dht22", t, temp, hum);
  bulkOutputDataPoints(env, n, "dht22", t);
  return true;
}