#include <Arduino.h>

#define TOGGLE_PIN 0

char WIFI_SSID[] = "";
char WIFI_PASSWORD[] = "";

char MQTT_HOST[] = "";
int MQTT_PORT = 0;
char MQTT_USER[] = "";
char MQTT_PASS[] = "";
char MQTT_TOPIC_DO[] = "";
char MQTT_TOPIC_REF[] = "";
char MQTT_TOPIC_VAL[] = "";

char deviceID[] = "";
bool rainSoon;

/* Сервер */

void setupServer() {
    // Раздать сеть и настроить сервер
}

void handle_get() {
    // Выдать форму пользователю
}

void handle_post() {
    // Обработать данные формы
}

/* API */

int registerDevice() {
    // Зарегистрировать устройство на платформе
    return 0;
}

bool getWeather() {
    // Узнать, будет ли дождь в ближайшие сутки
    return false;
}

/* Настройки */

void saveConfig() {
    // Сохранить конфиг
}

void loadConfig() {
    // Загрузить конфиг
}

/* MQTT */

void onMQTT() {
    // Получить сообщение с платформы
}

void parseMQTT() {
    // Обработать команду от платформы
}

void sendMQTT() {
    // Отправить сообщение на платформу
}

void sendMoisture() {
    // Передать влажность почвы
}

void sendStatus() {
    // Передать статус полива
}

/* ZigBee */

void onXBee() {
    // Получить сообщение от устройства
}

void parseXBee() {
    // Обработать сообщение устройства
}

void sendXBee() {
    // Отправить сообщение устройству
}

void sendMode() {
    // Передать режим полива
}

void sendReference() {
    // Передать пороговую влажность
}

void sendWeather() {
    // Передать информацию о дожде
}

/* База */

void setup() {
    // Инициализация
    /* Если зажата кнопка - в режиме сервера
     * Если не зажата - в режиме клиента */
}

void loop() {
    // Рабочий цикл
}
