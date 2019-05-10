#include <BME280I2C.h>                //https://github.com/finitespace/BME280
#include <EnvironmentCalculations.h>  //https://github.com/finitespace/BME280

#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>

bool readBme280(byte address){
  unsigned long int t;
  
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

  size_t tries = 0;
  do {
    delay(250);
  } while (tries++ <= 10 && !bme.begin());

  if (tries >= 10) {
    Serial.println("Tried too many times to communicate with bme280");
    return false;
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
    Serial.println("Read From bme280");
    size_t tries = 0;
    do {
      t = timeClient.getEpochTime();
      bme.read(pres, temp, hum, tempUnit, presUnit);
      delay(100);
    } while (tries++ <= 10 && !(temp >= -30.0f));

    if (tries >= 10) return false;
    
    DataPoint dps[4];
    memset(dps, 0, sizeof(dps));

    dps[0] = createDataPoint(FLOAT, "airPressure", "bme280", pres, t);
    dps[1] = createDataPoint(FLOAT, "airTemperature", "bme280", temp, t);
    dps[2] = createDataPoint(FLOAT, "airRelativeHumidity", "bme280", hum, t);
    double eslp = EnvironmentCalculations::EquivalentSeaLevelPressure(ALTITUDECONSTANT, temp, pres, envAltUnit, envTempUnit);
    dps[3] = createDataPoint(FLOAT,"airEquivalentSeaLevelPressure", "bme280", eslp, t);
    bulkOutputDataPoints(dps, 4, "bme280", t);
    
    DataPoint env[8];
    size_t n = createEnvironmentData(env, "bme280", t, temp, hum, pres);
    bulkOutputDataPoints(env, n, "bme280", t);
  }
  
  if (bme.chipModel() == BME280::ChipModel_BMP280){
    Serial.println("Read From bmp280");
    size_t tries = 0;
    do {
      t = timeClient.getEpochTime();
      bme.read(pres, temp, hum, tempUnit, presUnit);
      delay(100);
    } while (tries++ <= 10 && !(temp >= -30.0f));
    if (tries >= 10) return false;
    
    DataPoint dps[3];
    memset(dps, 0, sizeof(dps));

    dps[0] = createDataPoint(FLOAT,"airPressure", "bmp280", pres, t);
    dps[1] = createDataPoint(FLOAT,"airTemperature", "bmp280", temp, t);
    double eslp = EnvironmentCalculations::EquivalentSeaLevelPressure(ALTITUDECONSTANT, temp, pres, envAltUnit, envTempUnit);
    dps[2] = createDataPoint(FLOAT,"airEquivalentSeaLevelPressure", "bmp280", eslp, t);
    bulkOutputDataPoints(dps, 3, "bmp280", t);
    // no extra environment values
  }
  return true; 
}

bool readBme680(){
  unsigned long int t;
  Adafruit_BME680 bme; // I2C
  size_t tries = 0;
  do {
    delay(250);
  } while (tries++ <= 10 && !bme.begin());

  if (tries >= 10) {
    Serial.println("Tried too many times to communicate with bme680");
    return false;
  }

  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320*C for 150 ms

  Serial.println("Read From bme680");
  tries = 0;
  do {
    t = timeClient.getEpochTime();
    delay(250);
  } while (tries++ <= 10 && !bme.performReading());

  if (tries >= 10) return false;

  t = timeClient.getEpochTime(); // if we got here, reading succeeded and the time is now
  DataPoint dps[5];
  memset(dps, 0, sizeof(dps));
  float pres = bme.pressure;
  float temp = bme.temperature;
  float hum = bme.humidity;
  unsigned int gas = bme.readGas();
  dps[0] = createDataPoint(FLOAT, "airPressure", "bme680",pres , t);
  dps[1] = createDataPoint(FLOAT, "airTemperature", "bme680",temp , t);
  dps[2] = createDataPoint(FLOAT, "airRelativeHumidity", "bme680",hum , t);
  // gas resistance in Ohms
  dps[3] = createDataPoint(INT, "airGasResistance", "bme680", gas, t); 
  
  EnvironmentCalculations::TempUnit     envTempUnit =  EnvironmentCalculations::TempUnit_Celsius;
  EnvironmentCalculations::AltitudeUnit envAltUnit  =  EnvironmentCalculations::AltitudeUnit_Meters;
  double eslp = EnvironmentCalculations::EquivalentSeaLevelPressure(ALTITUDECONSTANT, temp, pres, envAltUnit, envTempUnit);
  dps[4] = createDataPoint(FLOAT,"airEquivalentSeaLevelPressure", "bme680", eslp, t);
  bulkOutputDataPoints(dps, 5, "bme680", t);
  
  DataPoint env[8];
  size_t n = createEnvironmentData(env, "bme680", t, temp, hum, pres);
  bulkOutputDataPoints(env, n, "bme680", t);

  return true;
}