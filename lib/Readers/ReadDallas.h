#include <OneWire.h>           // https://github.com/milesburton/Arduino-Temperature-Control-Library
#include <DallasTemperature.h> // https://github.com/milesburton/Arduino-Temperature-Control-Library

#define MAX_TRIES 20
#define DALLAS_MAX_TEMP 84
#define DALLAS_MIN_TEMP -10


// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_PIN);

bool isValidDallas(float temperature){
  if (isnan(temperature)) return false;
  if (temperature == DEVICE_DISCONNECTED_C) return false;
  if (temperature < DALLAS_MIN_TEMP || temperature > DALLAS_MAX_TEMP) return false;
  return true;
}

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);
DeviceAddress addr;
void readDallas(unsigned long int t){
  sensors.begin();
  Serial.println("Read From dallas");
  sensors.requestTemperatures();
  size_t numSensors = sensors.getDeviceCount();
  char addrHex[16];
  for (size_t x = 0; x < numSensors; x++){
    
    if (!sensors.getAddress(addr, x)) {
      Serial.printf("Unable to get device address for device %d/%d\n", x, numSensors);
      continue;
    }
    // set resolution to 11 bits
    sensors.setResolution(addr, 11);
    // get the address as hex
    sprintf(addrHex, "%016X", (unsigned int)addr);
    float temp;
    size_t tries = 0;
    do {
      temp = sensors.getTempC(addr);
      Serial.printf("Attempted to get temperature from Dallas %d/%d, got %.6f\n", x+1, numSensors, temp);
      delay(50);
    } while (tries++ < MAX_TRIES && !isValidDallas(temp));
    
    if (tries >= MAX_TRIES) continue;
    DataSender<DataPoint> sender(t, 1, "dallas", addrHex);
    DataPoint d = createDataPoint(FLOAT, "temperature", "dallas", addrHex, temp, t);
    sender.push_back(d);
  }
}