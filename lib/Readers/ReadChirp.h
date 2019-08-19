#include <I2CSoilMoistureSensor.h>    //https://github.com/Apollon77/I2CSoilMoistureSensor https://www.tindie.com/products/miceuz/i2c-soil-moisture-sensor/

#define MAX_TRIES 5

I2CSoilMoistureSensor chirpSensor;

#define CHIRP_MAX_TEMP 200
#define CHIRP_MIN_TEMP -100
#define CHIRP_MAX_CAP 1000
#define CHIRP_MIN_CAP 0


bool isValidChirp(unsigned int soilCapacitance, float soilTemperature){
  /**
   * Checks to see if values recieved from a Chirp sensor are valid / within the bounds set by 
   * the defines CHIRP_MAX_TEMP CHIRP_MIN_TEMP CHIRP_MAX_CAP CHIRP_MIN_CAP 
   */
  if (soilCapacitance > CHIRP_MAX_CAP || soilCapacitance < CHIRP_MIN_CAP){
    return false;
  }
  if (soilTemperature > CHIRP_MAX_TEMP || soilTemperature < CHIRP_MIN_TEMP){
    return false;
  }
  return true;
}

bool readChirp(){
  /**
   * reads values from a chirp sensor and adds the data to the sender.
   * requires that the ESP has clock stretching enabled through 
   * `Wire.setClockStretchLimit(2500);` ... Maybe.
   * 
   * @return bool whether the sensor was successfully read and datapoints added to sender
   */

  // Wire.setClockStretchLimit(2500);
  // chirp soil moisture sensor is address 0x20
  chirpSensor.begin(true); // wait needs 1s for startup
  uint8_t version = chirpSensor.getVersion();
  Serial.printf("read chirp v%02X\n", version);
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
  time_t t;
  size_t tries = 0;
  do {
    t = time(nullptr);
    soilCapacitance = chirpSensor.getCapacitance();
    soilTemperature = chirpSensor.getTemperature()/(float)10; 
    delay(100);
  } while (tries++ < MAX_TRIES && !isValidChirp(soilCapacitance, soilTemperature));
  if (tries >= MAX_TRIES) return false;
  DataSender<DataPoint> sender(3);

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
