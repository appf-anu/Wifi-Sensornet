#include <I2CSoilMoistureSensor.h>    //https://github.com/Apollon77/I2CSoilMoistureSensor https://www.tindie.com/products/miceuz/i2c-soil-moisture-sensor/

#define MAX_TRIES 5

I2CSoilMoistureSensor chirpSensor;

#define MAX_TEMP 200
#define MIN_TEMP -100
#define MAX_CAP 1000
#define MIN_CAP 0


bool readChirp(){
  // chirp soil moisture sensor is address 0x20
  unsigned long int t;
  chirpSensor.begin(true); // wait needs 1s for startup
  size_t waits = 0;
  do {
    delay(50);
  } while (chirpSensor.isBusy() && waits < 10);


  unsigned int soilCapacitance;
  float soilTemperature; 
  size_t tries = 0;
  do {
    t = timeClient.getEpochTime();
    soilCapacitance = chirpSensor.getCapacitance();
    soilTemperature = chirpSensor.getTemperature()/(float)10; 
    delay(100);
  } while (tries++ < MAX_TRIES && 
          !( soilCapacitance < MAX_CAP && soilCapacitance > MIN_CAP) &&
          !( soilTemperature < MAX_TEMP && soilTemperature > MIN_TEMP));
  if (tries >= MAX_TRIES) return false;
  
  DataPoint dps[2];
  memset(dps, 0, sizeof(dps));
  dps[0] = createDataPoint(INT, "soilCapacitance", "chirp", soilCapacitance, t);
  // temperature is in multiple of 10
  
  dps[1] = createDataPoint(FLOAT, "soilTemperature", "chirp", soilTemperature, t);

  bulkOutputDataPoints(dps, 2, "chirp", t);
  chirpSensor.sleep();
  return true;
}
