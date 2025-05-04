#include <Arduino.h>

typedef enum { OFF, AUTO, ON } Mode;

double value;
double reference;
bool rainSoon;
Mode mode;

/* Растения */

unsigned int getValue() {
    // Получить среднюю влажность почвы
    return 0;
}

void setReference() {
    // Установить пороговое значение влажности
}

bool shouldWater() {
    // Принять решение о поливе растений
    return false;
}

void waterPlants() {
    // Полить растения
}

/* ZigBee */

void onXBee() {
    // Получить сообщение от хаба
}

void parseMessage() {
    // Обработать команду
}

void sendXBee() {
    // Отправить сообщение хабу
}

void sendMoisture() {
    // Передать влажность почвы
}

void sendStatus() {
    // Передать статус полива
}

/* База */

void setup() {
    // Инициализация
}

void loop() {
    // Рабочий цикл
}
