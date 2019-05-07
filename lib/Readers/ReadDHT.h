#include <DHT.h>              //https://github.com/adafruit/DHT-sensor-library
#include <Adafruit_Sensor.h>  //https://github.com/adafruit/DHT-sensor-library

DHT dht(DHTPIN, DHT22);
void readDHT(){
  unsigned long int t = timeClient.getEpochTime();
  dht.begin();
  
  float hum = dht.readHumidity();
  float temp = dht.readTemperature();
  
  if (isnan(temp) || isnan(hum)){
    return;
  }
  DataPoint dps[2];
  Serial.println("Read From DHT22");
  
  dps[0] = createDataPoint(FLOAT, "airTemperature", "dht22", temp, t);
  dps[1] = createDataPoint(FLOAT, "airRelativeHumidity", "dht22", hum, t);
  bulkOutputDataPoints(dps, 2, "dht22", t);
  DataPoint env[6];
  size_t n = createEnvironmentData(env, "dht22", t, temp, hum);
  bulkOutputDataPoints(env, n, "dht22", t);
}