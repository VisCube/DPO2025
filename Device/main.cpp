#include <Arduino.h>
#include <XBee.h>
#include <ArduinoJson.h>
#include <string>

typedef enum { OFF, AUTO, ON } Mode;

// ===== Настройки =====
#define DEBUG true               
#define LED_PIN PC13            
#define SEND_INTERVAL 2000      // Интервал отправки сообщений в мс
#define BAUD_RATE 9600          // Скорость serial
// Сообщения от хаба
#define MSG_WATER_PLANTS 1      // Полить
#define MSG_SET_REFERENCE 2     // Поставить эталонное значение
#define MSG_SEND_WEATHER 3      // Передать погоду

// ===== Глобальные переменные =====
// Данные датчиков и настройки
double value;                  // Текущая влажность почвы
double reference = 50;         // Эталонное (пороговое) значение влажности (по умолчанию 50%)
bool rainSoon;                 // Флаг ожидания дождя
Mode mode = AUTO;              // Текущий режим работы

// XBee и сетевые настройки
XBeeWithCallbacks xbeeClient;
XBeeAddress64 xbeeAddr(0x00000000, 0x0000FFFF);  
HardwareSerial xbeeSerial(PA3, PA2);

// Буферы и данные
uint8_t* rx_data;              // Данные, полученные от XBee
uint8_t rx_length;             // Длина полученных данных
const char* DEFAULT_MESSAGE = "Hello from Arduino!";

// ===== Прототипы функций =====
// Функции растений
unsigned int getValue();
bool shouldWater();
void waterPlants();
void setReference(uint8_t* data, uint8_t length);

// Функции XBee
void zbReceive(ZBRxResponse &rx, uintptr_t);
void receiveMessage(uint8_t* data, uint8_t length);
void parseMessage(const char* command, const char* value);
void zbSend(uint8_t *payload, const uint8_t payloadLength);
void sendMessage(const char* message);
void sendMoisture();
void sendStatus();
void sendXBee();

// Утилиты
void blinkLED(int times = 1, int delayMs = 100);
void debugPrint(const char* message);
void debugPrintln(const char* message);

/* ===== Функции по работе с растениями ===== */

/**
 * Получает значение влажности почвы от датчиков
 * @return Значение влажности в процентах (0-100%)
 */
unsigned int getValue() {
    // Получить среднюю влажность почвы
    // TODO: Реализовать считывание данных с сенсора(ов) влажности почвы
    // и вернуть усредненное значение в процентах (0-100%)
    return 0;
}

/**
 * Устанавливает референсное значение влажности
 * @param value Новое значение влажности (0-100%)
 */
 void setReference(int newValue) {
    // Установить пороговое значение влажности
    if (newValue >= 0 && newValue <= 100) {
        reference = newValue;
        debugPrint("Установлено новое эталонное значение влажности: ");
        Serial.println(reference);
        
        blinkLED(2, 100);
    } else {
        debugPrintln("Ошибка: значение должно быть от 0 до 100");
    }
}

/**
 * Определяет, нужно ли поливать растения, на основе данных датчиков и настроек
 * @return true, если нужен полив, false - если нет
 */
bool shouldWater() {
    if (mode == ON) {
        // Если режим включен вручную, всегда поливаем
        return true;
    }

    // Если режим AUTO, проверяем условия для полива
    if (mode == AUTO) {
        // TODO: Реализовать логику принятия решения о поливе на основе сенсоров влажности
        // и погодных данных (rainSoon)
        return value < reference && !rainSoon;
    }

    // В режиме OFF полив не требуется
    return false;
}

/**
 * Выполняет полив растений
 */
void waterPlants() {
    // Полить растения
    debugPrintln("Полив...");
    // TODO: Реализовать управление насосом/клапаном для полива растений
    // с учетом заданной длительности полива и периодической проверкой влажности
}

/* ===== Функции по работе с XBee ===== */

/**
 * Базовый метод для приема данных от XBee-модуля
 * @param rx Полученные данные
 * @param Неиспользуемый параметр (требуется библиотекой XBee)
 */
void zbReceive(ZBRxResponse &rx, uintptr_t) {
    if (DEBUG) {
        Serial.print("Received [");
        Serial.print(rx.getDataLength());
        Serial.print(" bytes]: ");
    }

    rx_data = rx.getData();
    rx_length = rx.getDataLength();
    
    for (int i = 0; i < rx.getDataLength(); i++) {
        Serial.write(rx_data[i]);
    }
    Serial.println();
    
    if (rx.getDataLength() > 0) {
        receiveMessage(rx_data, rx_length);
    }
    
    blinkLED(3, 50);
}

/**
 * Обертка для сообщений из zbReceive для обработки
 * @param data Массив байт с данными
 * @param length Длина массива данных
 */
void receiveMessage(uint8_t* data, uint8_t length) {
    if (length <= 0) return;
    
    char message[length + 1];
    memcpy(message, data, length);
    message[length] = '\0';

    // Находим знак '='
    char* equalSign = strchr(message, '=');
    if (equalSign == NULL) {
        debugPrintln("Ошибка: неверный формат команды, отсутствует '='");
        return;
    }
    
    // Получаем префикс команды (до знака '=')
    int prefixLength = equalSign - message;
    char prefix[prefixLength + 1];
    strncpy(prefix, message, prefixLength);
    prefix[prefixLength] = '\0';
    
    // Получаем значение (после знака '=')
    char* value = equalSign + 1;
    
    parseMessage(prefix, value);
}

/**
 * Разбор и обработка полученной команды
 * @param commandType Тип команды для выполнения
 */
 void parseMessage(const char* command, const char* value) {
    if (strcmp(command, "MODE") == 0) {
        debugPrint("Команда: Установка режима на ");
        debugPrintln(value);
        
        if (strcmp(value, "OFF") == 0) {
            mode = OFF;
        } else if (strcmp(value, "AUTO") == 0) {
            mode = AUTO;
        } else if (strcmp(value, "ON") == 0) {
            mode = ON;
        } else {
            debugPrintln("Ошибка: неизвестное значение режима");
        }
        
        blinkLED(2, 100);
    }
    else if (strcmp(command, "STATUS") == 0) {
        if (strcmp(value, "?") == 0) {
            sendStatus();
        }
    }
    else if (strcmp(command, "REFERENCE") == 0) {
        debugPrintln("Команда: Задать эталонное значение влажности");
        
        int newValue = atoi(value);
        setReference(newValue);
    }
    else if (strcmp(command, "VALUE") == 0) {
        if (strcmp(value, "?") == 0) {
            sendMoisture();
        }
    }
    else {
        debugPrint("Неизвестная команда: ");
        Serial.println(command);
    }
}

/**
 * Базовый метод отправки данных через XBee-модуль
 * @param payload Массив байт для отправки
 * @param payloadLength Длина массива данных
 */
void zbSend(uint8_t *payload, const uint8_t payloadLength) {
    ZBTxRequest tx(xbeeAddr, payload, payloadLength);
    xbeeClient.send(tx);
    
    if (DEBUG) {
        Serial.print("Sent [");
        Serial.print(payloadLength);
        Serial.print(" bytes]: ");
        for (int i = 0; i < payloadLength; i++) {
            Serial.write(payload[i]);
        }
        Serial.println();
    }
    
    blinkLED(1, 50);
}

/**
 * Обертка над zbSend для отправки текстовых сообщений
 * @param message Текстовое сообщение для отправки
 */
void sendMessage(const char* message) {
    // Отправить сообщение хабу
    size_t length = strlen(message);
    uint8_t data[length];
    memcpy(data, message, length);
    zbSend(data, length);
}

/**
 * Отправка стандартного сообщения (для проверки связи)
 */
void sendXBee() {
    // Отправить стандартное сообщение хабу
    sendMoisture();
}

/**
 * Отправка данных о влажности почвы
 */
void sendMoisture() {
    // Передать влажность почвы
    debugPrintln("Отправка данных о влажности...");
    
    // TODO: Заменить на реальное значение от сенсора влажности почвы
    unsigned int sensorValue = getValue();
    value = sensorValue;
    
    char moistureStr[30];
    sprintf(moistureStr, "VALUE=%d", (int)value);
    sendMessage(moistureStr);
}

/**
 * Отправка общего статуса системы полива
 */
void sendStatus() {
    // Передать статус полива
    debugPrintln("Отправка статуса полива...");
    
    // Отправляем текущий режим
    char modeStr[15];
    sprintf(modeStr, "MODE=%s", mode == OFF ? "OFF" : (mode == AUTO ? "AUTO" : "ON"));
    sendMessage(modeStr);
    
    // Отправляем статус полива
    char statusStr[15];
    sprintf(statusStr, "STATUS=%s", (mode == ON || (mode == AUTO && value < reference)) ? "ON" : "OFF");
    sendMessage(statusStr);
    
    // Отправляем пороговое значение влажности
    char referenceStr[20];
    sprintf(referenceStr, "REFERENCE=%d", (int)reference);
    sendMessage(referenceStr);
    
    // Отправляем текущее значение влажности
    sendMoisture();
}

/* ===== Основные функции Arduino ===== */

/**
 * Инициализация устройства и подсистем
 */
void setup() {
    // Инициализация
    #ifdef LED_PIN
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);  
    #endif
    
    Serial.begin(BAUD_RATE);
    while (!Serial && millis() < 3000);
    debugPrintln("Serial initialized");
    
    xbeeSerial.begin(BAUD_RATE);
    xbeeClient.setSerial(xbeeSerial);
    xbeeClient.onZBRxResponse(zbReceive);
    debugPrintln("XBee initialized");
    
    debugPrintln("Setup complete - ready to communicate");
    blinkLED(2, 200); 
}

/**
 * Основной рабочий цикл
 */
void loop() {
    // Обработка входящих XBee сообщений
    xbeeClient.loop();

    // Получение значения влажности
    value = getValue();
    
    // Периодическая отправка данных
    static unsigned long lastSendTime = 0;
    unsigned long currentTime = millis();

    if (currentTime - lastSendTime > SEND_INTERVAL) {
        sendXBee();
        lastSendTime = currentTime;
    }

    
    // Проверка необходимости полива
    if (shouldWater()) {
        waterPlants();
    }
    
    delay(10);
}

/* ===== Утилиты ===== */

/**
 * Мигает LED-индикатором указанное количество раз
 * @param times Количество миганий
 * @param delayMs Длительность одного мигания в мс
 */
void blinkLED(int times, int delayMs) {
    #ifdef LED_PIN
    for (int i = 0; i < times; i++) {
        digitalWrite(LED_PIN, LOW);   
        delay(delayMs);
        digitalWrite(LED_PIN, HIGH); 
        if (i < times - 1) delay(delayMs);
    }
    #endif
}

/**
 * Вывод отладочного сообщения в Serial (если DEBUG=true)
 * @param message Сообщение для вывода
 */
void debugPrint(const char* message) {
    if (DEBUG) Serial.print(message);
}

/**
 * Вывод отладочного сообщения с переводом строки в Serial (если DEBUG=true)
 * @param message Сообщение для вывода
 */
void debugPrintln(const char* message) {
    if (DEBUG) Serial.println(message);
}