#ifndef CONSTANTS_H
#define CONSTANTS_H
#include "Structs.h"
#endif

#define PIN_XBEE_RX 27
#define PIN_XBEE_TX 26

constexpr int PIN_MOIST[] = {A0, A1, A2, A3, A4, A5, A6, A7};
#define PIN_WATER A8
#define PIN_WATERING A9

constexpr char XBEE_COMMAND_REFERENCE[] = "REFERENCE";
constexpr char XBEE_COMMAND_VALUE[] = "VALUE";
constexpr char XBEE_COMMAND_MODE[] = "MODE";
constexpr char XBEE_COMMAND_STATUS[] = "STATUS";
constexpr char XBEE_COMMAND_WATER[] = "WATER";
constexpr char XBEE_COMMAND_RAIN[] = "RAIN";

constexpr char MODE_OFF[] = "1";
constexpr char MODE_ON[] = "2";
constexpr char MODE_AUTO[] = "3";

constexpr long EEPROM_SIZE = sizeof(Config);
constexpr long UPDATE_INTERVAL = 1000l * 60l;
constexpr long WEATHER_INTERVAL = 1000l * 60l * 60l * 24l;

constexpr int MIN_MOIST = 1;
