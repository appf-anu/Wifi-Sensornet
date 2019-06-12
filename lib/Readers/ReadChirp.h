#include <I2CSoilMoistureSensor.h>    //https://github.com/Apollon77/I2CSoilMoistureSensor https://www.tindie.com/products/miceuz/i2c-soil-moisture-sensor/

#define MAX_TRIES 5

I2CSoilMoistureSensor chirpSensor;

#define CHIRP_MAX_TEMP 200
#define CHIRP_MIN_TEMP -100
#define CHIRP_MAX_CAP 1000
#define CHIRP_MIN_CAP 0


bool isValidChirp(unsigned int soilCapacitance, float soilTemperature){
  if (soilCapacitance > CHIRP_MAX_CAP || soilCapacitance < CHIRP_MIN_CAP){
    return false;
  }
  if (soilTemperature > CHIRP_MAX_TEMP || soilTemperature < CHIRP_MIN_TEMP){
    return false;
  }
  return true;
}

bool readChirp(unsigned long int t){
  // Wire.setClockStretchLimit(2500);
  // chirp soil moisture sensor is address 0x20
  chirpSensor.begin(true); // wait needs 1s for startup
  uint8_t version = chirpSensor.getVersion();
  Serial.printf("Read from chirp v%02X\n", version);
  if (version >= 23){
    chirpSensor.getCapacitance();
    chirpSensor.getTemperature();
    size_t waits = 0;
    do {
      delay(50);
    } while (chirpSensor.isBusy() && waits < 10);
    if (waits >= 10) return false;
  }

  unsigned int soilCapacitance;
  float soilTemperature; 
  size_t tries = 0;
  do {
    soilCapacitance = chirpSensor.getCapacitance();
    soilTemperature = chirpSensor.getTemperature()/(float)10; 
    delay(100);
    yield();
  } while (tries++ < MAX_TRIES && !isValidChirp(soilCapacitance, soilTemperature));
  if (tries >= MAX_TRIES) return false;
  DataSender<DataPoint> sender(t, 3, "chirp");

  DataPoint soilCapacitance_ = createDataPoint(INT, "soilCapacitance", "chirp", soilCapacitance, t); 
  sender.push_back(soilCapacitance_);
  // temperature is in multiple of 10
  DataPoint soilTemperature_ = createDataPoint(FLOAT, "soilTemperature", "chirp", soilTemperature, t);
  sender.push_back(soilTemperature_);
  
  // this breaks other i2c devices.
  if (version >= 23) chirpSensor.sleep();
  
  // Wire.setClockStretchLimit(230);
  // .../framework-arduinoespressif8266/cores/esp8266/core_esp8266_si2c.cpp:161
  // twi_setClockStretchLimit(230); // default value is 230 uS
  return true;
}
