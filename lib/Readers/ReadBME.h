#include <BME280I2C.h>                //https://github.com/finitespace/BME280
#include <EnvironmentCalculations.h>  //https://github.com/finitespace/BME280

#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_BME680.h>


#define MAX_TRIES 5

#define MAX_TEMP 200
#define MIN_TEMP -100
#define MAX_HUM 100
#define MIN_HUM 0

Adafruit_BME280 bme;

bool readBme280(byte address){
  unsigned long int t;
  
  size_t tries = 0;
  do {
    delay(250);
  } while (tries++ < MAX_TRIES && !bme.begin(address));

  if (tries >= MAX_TRIES) {
    Serial.println("Tried too many times to communicate with bme280");
    return false;
  }

  delay(250);
  float temp = 0;
  float hum  = 0;
  float pres = 0;
  // TODO: add bme680 here
  bme.setSampling(Adafruit_BME280::MODE_FORCED,
                  Adafruit_BME280::SAMPLING_X1, // temperature
                  Adafruit_BME280::SAMPLING_X1, // pressure
                  Adafruit_BME280::SAMPLING_X1, // humidity
                  Adafruit_BME280::FILTER_OFF   );

  // whatever is going on here it gives weird values.
  Serial.println("Read From bme280");
  tries = 0;
  do {
    t = timeClient.getEpochTime();
    temp = bme.readTemperature();
    pres = bme.readPressure()/100.0F;
    hum = bme.readHumidity();
    delay(100);
  } while (tries++ < MAX_TRIES && 
        !(temp < MAX_TEMP && temp > MIN_TEMP) &&
        !(hum < MAX_HUM && hum > MIN_HUM));

  if (tries >= MAX_TRIES) return false;
  
  DataPoint dps[3];
  memset(dps, 0, sizeof(dps));

  dps[0] = createDataPoint(FLOAT, "airPressure", "bme280", pres, t);
  dps[1] = createDataPoint(FLOAT, "airTemperature", "bme280", temp, t);
  dps[2] = createDataPoint(FLOAT, "airRelativeHumidity", "bme280", hum, t);

  bulkOutputDataPoints(dps, 3, "bme280", t);
  
  size_t n = createEnvironmentData("bme280", t, temp, hum, pres);
  bulkOutputDataPoints(env, n, "bme280", t);

  return true; 
}

bool readBme680(){
  unsigned long int t;
  Adafruit_BME680 bme; // I2C
  size_t tries = 0;
  do {
    delay(250);
  } while (tries++ < MAX_TRIES && !bme.begin());

  if (tries >= MAX_TRIES) {
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
  float pres;
  float temp;
  float hum;
  unsigned int gas;
  do {
    t = timeClient.getEpochTime();
    temp = bme.readTemperature();
    pres = bme.readPressure()/100.0F;
    hum = bme.readHumidity();
    gas = bme.readGas();
    delay(250); // wait a little bit longer
  } while (tries++ < MAX_TRIES && 
          !(temp < MAX_TEMP && temp > MIN_TEMP) &&
          !(hum < MAX_HUM && hum > MIN_HUM));

  if (tries >= MAX_TRIES) return false;

  DataPoint dps[5];
  memset(dps, 0, sizeof(dps));  
  dps[0] = createDataPoint(FLOAT, "airPressure", "bme680", pres, t);
  dps[1] = createDataPoint(FLOAT, "airTemperature", "bme680",temp , t);
  dps[2] = createDataPoint(FLOAT, "airRelativeHumidity", "bme680",hum , t);
  
  // gas resistance in Ohms
  dps[3] = createDataPoint(INT, "airGasResistance", "bme680", gas, t); 
  
  bulkOutputDataPoints(dps, 5, "bme680", t);
  
  size_t n = createEnvironmentData("bme680", t, temp, hum, pres);
  bulkOutputDataPoints(env, n, "bme680", t);

  return true;
}