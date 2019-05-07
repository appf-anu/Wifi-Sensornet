#include <I2CSoilMoistureSensor.h>    //https://github.com/Apollon77/I2CSoilMoistureSensor https://www.tindie.com/products/miceuz/i2c-soil-moisture-sensor/

I2CSoilMoistureSensor chirpSensor;
void readChirp(){
  // chirp soil moisture sensor is address 0x20
  unsigned long int t = timeClient.getEpochTime();
  chirpSensor.begin(true); // wait needs 1s for startup
  while (chirpSensor.isBusy()) delay(50);
  unsigned int soilCapacitance = chirpSensor.getCapacitance();
  DataPoint dps[2];
  memset(dps, 0, sizeof(dps));
  dps[0] = createDataPoint(INT, "soilCapacitance", "chirp", soilCapacitance, t);
  // temperature is in multiple of 10
  float soilTemperature = chirpSensor.getTemperature()/(float)10; 
  dps[1] = createDataPoint(FLOAT, "soilTemperature", "chirp", soilTemperature, t);

  bulkOutputDataPoints(dps, 2, "chirp", t);
  chirpSensor.sleep();
}
