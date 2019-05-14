#include <FS.h>
#include <Arduino.h>


struct Config{
  //define your default values here, if there are different values in config.json, they are overwritten.
  char influxdb_server[64] = "influxdb.traitcapture.org";
  char influxdb_port[6] = "8086";
  char influxdb_user[16] = "iot";
  char influxdb_db[16] = "sensornet";
  char influxdb_password[32] = "influxdb password";
  char location[16] = "unknown";
  char interval[6] = "1";
};

int loadConfig(Config *cfg){
    if (!SPIFFS.exists("/config.dat")) {
        Serial.println("No config file.");
        return 1;
    }
    //file exists, reading and loading
    Serial.println("reading config file");
    File f = SPIFFS.open("/config.dat", "r");
    if (!f) {
        Serial.println("failed to load json config");
        return 1;
    }
    Serial.printf("opening config file: %db\n", f.size());
    
    f.read((uint8_t *)cfg, sizeof(*cfg));
    Serial.println("Config file read.");
    f.close();
    return 0;
}

int saveConfig(Config *cfg){
    Serial.println("saving config");
    File f = SPIFFS.open("/config.dat", "w");
    if (!f) {
        Serial.println("failed to open config file for writing");
        return 1;
    }
    f.write((uint8_t *)cfg, sizeof(*cfg));
    f.close();
    return 0;
}