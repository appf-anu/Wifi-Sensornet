#include <FS.h>
#include <Arduino.h>
#include <ArduinoJson.h>

struct Config{
  //define your default values here, if there are different values in config.json, they are overwritten.
  char influxdb_server[64] = "influxdb.traitcapture.org";
  char influxdb_port[6] = "8086";
  char influxdb_user[16] = "user";
  char influxdb_db[16] = "database";
  char influxdb_password[32] = "your influxdb password";
  char location[16] = "Unknown";
  char interval[6] = "10";
};

int loadConfig(Config *cfg){
    if (!SPIFFS.exists("/config.json")) {
        Serial.println("No config file.");
        return 1;
    }
    //file exists, reading and loading
    Serial.println("reading config file");
    File configFile = SPIFFS.open("/config.json", "r");
    if (!configFile) {
        Serial.println("failed to load json config");
        return 1;
    }
    Serial.println("opened config file");
    size_t size = configFile.size();
    // Allocate a buffer to store contents of the file.
    std::unique_ptr<char[]> buf(new char[size]);

    configFile.readBytes(buf.get(), size);
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.parseObject(buf.get());
    json.printTo(Serial);
    if (!json.success()) {
        Serial.println("json decode failure.");
        return 1;
    }
    strcpy(cfg->influxdb_server, json["influxdb_server"]);
    strcpy(cfg->influxdb_port, json["influxdb_port"]);
    strcpy(cfg->influxdb_db, json["influxdb_db"]);
    strcpy(cfg->influxdb_user, json["influxdb_user"]);
    strcpy(cfg->influxdb_password, json["influxdb_password"]);
    strcpy(cfg->interval, json["interval"]);
    strcpy(cfg->location, json["location"]);
    configFile.close();

    return 0;
}

int saveConfig(Config *cfg){
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["influxdb_server"] = cfg->influxdb_server;
    json["influxdb_port"] = cfg->influxdb_port;
    json["influxdb_db"] = cfg->influxdb_db;
    json["influxdb_user"] = cfg->influxdb_user;
    json["influxdb_password"] = cfg->influxdb_password;
    json["interval"] = cfg->interval;
    json["location"] = cfg->location;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
        Serial.println("failed to open config file for writing");
        return 1;
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    return 0;
}