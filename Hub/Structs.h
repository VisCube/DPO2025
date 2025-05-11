#ifndef STRUCTS_H
#define STRUCTS_H
#endif

struct Config {
    char WIFI_SSID[64];
    char WIFI_PASSWORD[64];

    char MQTT_HOST[64];
    int MQTT_PORT;
    char MQTT_USERNAME[64];
    char MQTT_PASSWORD[64];

    int DEVICE_ID;
    char WQTT_TOKEN[64];

    float WEATHER_LATITUDE;
    float WEATHER_LONGITUDE;
};
