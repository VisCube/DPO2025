#include <Arduino.h>
#include <EEPROM.h>
#include <XBee.h>

#include "Constants.h"

HardwareSerial Serial2(PIN_XBEE_RX, PIN_XBEE_TX);
XBeeWithCallbacks xbeeClient;

Config config;
bool rainSoon;
unsigned long updateLast;
unsigned long weatherLast;

/* Настройки */

void loadConfig() {
    EEPROM.begin();
    EEPROM.get(0, config);
    EEPROM.end();

    if (config.reference == -1) config.reference = MIN_MOIST;
    if (config.mode == -1) config.mode = AUTO;
}

void saveConfig() {
    EEPROM.begin();
    EEPROM.put(0, config);
    EEPROM.end();
}

/* ZigBee */

void zbReceive(ZBRxResponse &rx, uintptr_t) {
    const int payloadLength = rx.getDataLength();
    char payload[payloadLength + 1];
    memcpy(payload, rx.getData(), payloadLength);
    payload[payloadLength] = '\0';

    Serial.print("ZigBee payload received: ");
    Serial.println(payload);

    const char *command = strtok(payload, "=");
    const char *value = strtok(nullptr, "=");

    if (strcmp(command, XBEE_COMMAND_REFERENCE) == 0) {
        config.reference = static_cast<int>(String(value).toInt());
        saveConfig();
    } else if (strcmp(command, XBEE_COMMAND_MODE) == 0) {
        config.mode = strcmp(value, MODE_OFF) == 0 ? OFF : strcmp(value, MODE_ON) == 0 ? ON : AUTO;
        saveConfig();
    } else if (strcmp(command, XBEE_COMMAND_RAIN) == 0) {
        rainSoon = strcmp(value, "1") == 0;
    }
}

void zbSend(const char *command, const char *value) {
    const auto commandLength = strlen(command);
    const auto valueLength = strlen(value);
    const auto payloadLength = commandLength + valueLength + 1;

    char buffer[payloadLength];
    sprintf(buffer, "%s=%s", command, value);
    const auto payload = reinterpret_cast<unsigned char *>(buffer);

    constexpr XBeeAddress64 address(0x00000000, 0x0000FFFF);
    ZBTxRequest tx(address, payload, payloadLength);
    xbeeClient.send(tx);
}

/* Растения */

bool shouldWater(const float moisture, const float water, const bool rainSoon, const Mode mode) {
    if (water == LOW) return false;

    if (mode == OFF) return false;
    if (mode == ON) return true;

    if (moisture < MIN_MOIST) return true;
    if (rainSoon) return false;
    return moisture < config.reference;
}

void checkPlants() {
    int moisture = 0;
    for (const int i: PIN_MOIST) moisture += static_cast<int>(map(analogRead(i), 0, 1023, 0, 100));
    moisture /= sizeof(PIN_MOIST);
    zbSend(XBEE_COMMAND_VALUE, String(moisture).c_str());

    const bool water = digitalRead(PIN_WATER) == LOW;
    zbSend(XBEE_COMMAND_WATER, String(water).c_str());

    const bool should = shouldWater(moisture, water, rainSoon, config.mode);
    zbSend(XBEE_COMMAND_STATUS, should ? "1" : "0");
    digitalWrite(PIN_WATERING, should);
}

/* База */

void setup() {
    Serial.begin(9600);
    Serial2.begin(9600);

    loadConfig();

    pinMode(PIN_WATER, INPUT_PULLUP);
    pinMode(PIN_WATERING, OUTPUT);

    xbeeClient.setSerial(Serial2);
    xbeeClient.onZBRxResponse(zbReceive);
}

void loop() {
    xbeeClient.loop();

    const unsigned long now = millis();
    if (now < updateLast || now - updateLast > UPDATE_INTERVAL) {
        checkPlants();
        updateLast = millis();
    }
    if (now < weatherLast || now - weatherLast > WEATHER_INTERVAL) {
        rainSoon = false;
        weatherLast = millis();
    }
}
