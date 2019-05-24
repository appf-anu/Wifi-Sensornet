#include <OneWire.h>           // https://github.com/milesburton/Arduino-Temperature-Control-Library
#include <DallasTemperature.h> // https://github.com/milesburton/Arduino-Temperature-Control-Library

#define MAX_TRIES 5
#define DALLAS_MAX_TEMP 125
#define DALLAS_MIN_TEMP -40


// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_PIN);

bool isValidDallas(float temperature){
  if (isnan(temperature)) return false;
  if (temperature < DALLAS_MIN_TEMP || temperature > DALLAS_MAX_TEMP) return false;
  return true;
}

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);
DeviceAddress addr;
bool readDallas(){
  bool failed = false;
  sensors.begin();
  Serial.println("Read From dallas");
  sensors.requestTemperatures();
  size_t numSensors = sensors.getDeviceCount();
  char addrHex[16];
  for (size_t x = 0; x < numSensors; x++){
    
    if (!sensors.getAddress(addr, x)) {
      Serial.printf("Unable to get device address for device %d/%d\n", x, numSensors);
      failed = true;
      continue;
    }
    // set resolution to 9 bits
    sensors.setResolution(addr, 9);
    // get the address as hex
    sprintf(addrHex, "%016X", (unsigned int)addr);
    float temp = sensors.getTempC(addr);

    if (temp == DEVICE_DISCONNECTED_C){
      Serial.printf("Couldn't get temperature data for device %d/%d\n", x, numSensors);
      failed = true;
      continue;
    }

    DataPoint d = createDataPoint(FLOAT, "temperature", "dallas", addrHex, temp);
    postDataPointToInfluxDB(&d);
  }

  return !failed;
}