#include <BME280I2C.h>                //https://github.com/finitespace/BME280
#include <EnvironmentCalculations.h>  //https://github.com/finitespace/BME280

#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_BME680.h>


#define MAX_TRIES 5

#define BME_MAX_TEMP 85
#define BME_MIN_TEMP -40
#define BME_MAX_HUM 100
#define BME_MIN_HUM 0
#define BME_MIN_PRES 300
#define BME_MAX_PRES 1100

Adafruit_BME280 bme;


bool isValidBME(float temperature, float humidity, float pressure){
  if (isnan(temperature) || isnan(humidity) || isnan(pressure)) return false;
  if (temperature < BME_MIN_TEMP || temperature > BME_MAX_TEMP) return false;
  if (humidity < BME_MIN_HUM || humidity > BME_MAX_HUM) return false;
  if (pressure < BME_MIN_PRES || pressure > BME_MAX_PRES) return false;
  return true;
}


bool readBme280(byte address){
  /**
   * reads values from a bme280 sensor and adds the data to the sender.
   * 
   * @param byte addr the i2c address of the sensor.
   * @return bool whether the sensor was successfully read and datapoints added to sender.
   */

  size_t tries = 0;
  do {
    delay(250);
  } while (tries++ < MAX_TRIES && !bme.begin(address));

  if (tries >= MAX_TRIES) {
    Serial.println("bme280 too many tries");
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
  Serial.println("read bme280");
  tries = 0;
  time_t t;
  do {
    t = time(nullptr);
    bme.takeForcedMeasurement();
    temp = bme.readTemperature();
    pres = bme.readPressure()/100.0F;
    hum = bme.readHumidity();
    delay(100);
  } while (tries++ < MAX_TRIES && !isValidBME(temp, hum, pres));

  if (tries >= MAX_TRIES) return false;
  
  DataSender<DataPoint> sender(3);

  DataPoint airPressure = createDataPoint(FLOAT, "airPressure", "bme280", pres, t);
  sender.push_back(airPressure);
  DataPoint airTemperature = createDataPoint(FLOAT, "airTemperature", "bme280",temp , t);
  sender.push_back(airTemperature);
  DataPoint airRelativeHumidity = createDataPoint(FLOAT, "airRelativeHumidity", "bme280",hum , t);
  sender.push_back(airRelativeHumidity);
  sender.push_environment_data(temp, hum, pres);
  return true; 
}

bool readBme680(){
  /**
   * reads values from a bme680 sensor and adds the data to the sender.
   * 
   * @return bool whether the sensor was successfully read and datapoints added to sender.
   */

  Adafruit_BME680 bme; // I2C
  size_t tries = 0;
  do {
    delay(250);
  } while (tries++ < MAX_TRIES && !bme.begin());

  if (tries >= MAX_TRIES) {
    Serial.println("bme680 too many tries");
    return false;
  }

  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320*C for 150 ms

  Serial.println("read bme680");
  tries = 0;
  float pres;
  float temp;
  float hum;
  unsigned int gas;
  time_t t;
  do {
    t = time(nullptr);
    temp = bme.readTemperature();
    pres = bme.readPressure()/100.0F;
    hum = bme.readHumidity();
    gas = bme.readGas();
    delay(250); // wait a little bit longer
  } while (tries++ < MAX_TRIES && !isValidBME(temp, hum, pres));

  if (tries >= MAX_TRIES) return false;

  DataSender<DataPoint> sender(3);

  DataPoint airPressure = createDataPoint(FLOAT, "airPressure", "bme680", pres, t);
  sender.push_back(airPressure);
  DataPoint airTemperature = createDataPoint(FLOAT, "airTemperature", "bme680",temp , t);
  sender.push_back(airTemperature);
  DataPoint airRelativeHumidity = createDataPoint(FLOAT, "airRelativeHumidity", "bme680",hum , t);
  sender.push_back(airRelativeHumidity);
  
  // gas resistance in Ohms
  DataPoint gas_ = createDataPoint(INT, "airGasResistance", "bme680", gas, t); 
  sender.push_back(gas_);
  sender.push_environment_data(temp, hum, pres);

  return true;
}