#include <BME280I2C.h>                //https://github.com/finitespace/BME280
#include <EnvironmentCalculations.h>  //https://github.com/finitespace/BME280

void readBme280(byte address){
  unsigned long int t;
  unsigned int tries = 0;
  BME280I2C::Settings settings(
   BME280::OSR_X1,
   BME280::OSR_X1,
   BME280::OSR_X1,
   BME280::Mode_Forced,
   BME280::StandbyTime_1000ms,
   BME280::Filter_16,
   BME280::SpiEnable_False,
   (address == 0x76 ? BME280I2C::I2CAddr_0x76:BME280I2C::I2CAddr_0x77)
  );
  BME280I2C bme(settings);
  
  while (!bme.begin()){
    if (tries >= 10) {
      Serial.println("Tried too many times to communicate with bme280");
      return;
    }
    tries++;
    delay(250);
  }

  delay(250);
  float temp = 0;
  float hum  = 0;
  float pres = 0;
  BME280::TempUnit tempUnit = BME280::TempUnit_Celsius;
  BME280::PresUnit presUnit = BME280::PresUnit_hPa;
  EnvironmentCalculations::TempUnit     envTempUnit =  EnvironmentCalculations::TempUnit_Celsius;
  EnvironmentCalculations::AltitudeUnit envAltUnit  =  EnvironmentCalculations::AltitudeUnit_Meters;
  // TODO: add bme680 here

  if (bme.chipModel() ==  BME280::ChipModel_BME280){
    // whatever is going on here it gives weird values.
    bme.read(pres, temp, hum, tempUnit, presUnit);
    delay(100);
    t = timeClient.getEpochTime();
    
    bme.read(pres, temp, hum, tempUnit, presUnit);
    Serial.println("Read From bme280");
    DataPoint dps[4];
    memset(dps, 0, sizeof(dps));

    dps[0] = createDataPoint(FLOAT, "airPressure", "bme280", pres, t);
    dps[1] = createDataPoint(FLOAT, "airTemperature", "bme280", temp, t);
    dps[2] = createDataPoint(FLOAT, "airRelativeHumidity", "bme280", hum, t);
    float eslp = EnvironmentCalculations::EquivalentSeaLevelPressure(ALTITUDECONSTANT, temp, pres, envAltUnit, envTempUnit);
    dps[3] = createDataPoint(FLOAT,"airEquivalentSeaLevelPressure", "bme280", eslp, t);
    bulkOutputDataPoints(dps, 4, "bme280", t);
    
    DataPoint env[8];
    size_t n = createEnvironmentData(env, "bme280", t, temp, hum, pres);
    bulkOutputDataPoints(env, n, "bme280", t);
  }

  if (bme.chipModel() == BME280::ChipModel_BMP280){
    // whatever is going on here it gives weird values.
    bme.read(pres, temp, hum, tempUnit, presUnit);
    delay(100);
    t = timeClient.getEpochTime();
    
    bme.read(pres, temp, hum, tempUnit, presUnit);
    
    Serial.println("Read From bmp280");
    DataPoint dps[3];
    memset(dps, 0, sizeof(dps));

    dps[0] = createDataPoint(FLOAT,"airPressure", "bmp280", pres, t);
    dps[1] = createDataPoint(FLOAT,"airTemperature", "bmp280", temp, t);
    float eslp = EnvironmentCalculations::EquivalentSeaLevelPressure(ALTITUDECONSTANT, temp, pres, envAltUnit, envTempUnit);
    dps[2] = createDataPoint(FLOAT,"airEquivalentSeaLevelPressure", "bmp280", eslp, t);
    bulkOutputDataPoints(dps, 3, "bmp280", t);
    // no extra environment values
  }
  
}
