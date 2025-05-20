#ifndef CONSTANTS_H
#define CONSTANTS_H
#include "Structs.h"
#endif

#define PIN_LED 2
#define PIN_MODE 21

constexpr char WIFI_SSID[] = "ESP32 Config";
constexpr char WIFI_PASSWORD[] = "";

constexpr char MQTT_TOPIC_REFERENCE[] = "moisture/reference";
constexpr char MQTT_TOPIC_VALUE[] = "moisture/value";
constexpr char MQTT_TOPIC_MODE[] = "watering/mode";
constexpr char MQTT_TOPIC_STATUS[] = "watering/status";
constexpr char MQTT_TOPIC_WATER[] = "water/level";

constexpr char XBEE_COMMAND_REFERENCE[] = "REFERENCE";
constexpr char XBEE_COMMAND_VALUE[] = "VALUE";
constexpr char XBEE_COMMAND_MODE[] = "MODE";
constexpr char XBEE_COMMAND_STATUS[] = "STATUS";
constexpr char XBEE_COMMAND_WATER[] = "WATER";
constexpr char XBEE_COMMAND_RAIN[] = "RAIN";

constexpr char ENDPOINT_DEVICE_CONNECT[] = "https://dash.wqtt.ru/api/broker";
constexpr char ENDPOINT_DEVICE_REGISTER[] = "https://dash.wqtt.ru/api/devices";

constexpr char ENDPOINT_WEATHER[] = "https://api.open-meteo.com/v1/forecast?latitude=%lat%&longitude=%lon%&hourly=rain&forecast_days=1";
constexpr char ENDPOINT_TIME[] = "https://aisenseapi.com/services/v1/timestamp";

constexpr long EEPROM_SIZE = sizeof(Config);
constexpr long WEATHER_INTERVAL = 1000l * 60l * 60l;
