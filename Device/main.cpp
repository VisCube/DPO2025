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
void parseMessage(uint8_t commandType);
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
 * Устанавливает референсное значение влажности на основе полученных данных
 * @param data Массив байт с командой и параметрами
 * @param length Длина массива данных
 */
void setReference(uint8_t* data, uint8_t length) {
    // Установить пороговое значение влажности
    if (length < 3) {
        debugPrintln("Ошибка: слишком короткие данные");
        return;
    }
    
    // Пропускаем первый байт (код команды)
    // Ищем пробел после кода команды
    int spaceIndex = -1;
    for (int i = 1; i < length; i++) {
        if (data[i] == ' ') {
            spaceIndex = i;
            break;
        }
    }
    
    if (spaceIndex == -1 || spaceIndex == length - 1) {
        debugPrintln("Ошибка: неверный формат команды");
        return;
    }
    
    // Преобразуем оставшуюся часть в число
    char valueStr[10] = {0};
    int valueLen = 0;
    
    for (int i = spaceIndex + 1; i < length && valueLen < 9; i++) {
        if (data[i] >= '0' && data[i] <= '9') {
            valueStr[valueLen++] = data[i];
        }
    }
    
    int newValue = atoi(valueStr);
    
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
    // Принять решение о поливе растений
    // TODO: Реализовать логику принятия решения о поливе на основе сенсоров влажности,
    // погодных данных (rainSoon)
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
    
    // Получаем первый байт как ASCII символ
    uint8_t commandType = data[0];
    
    // Преобразуем ASCII символ в число
    int commandValue;
    
    // Если это цифра в ASCII ('0'-'9')
    if (commandType >= '0' && commandType <= '9') {
        commandValue = commandType - '0';  // Преобразуем ASCII в число
    } else {
        // Если это не ASCII цифра, считаем что это уже бинарное значение
        commandValue = commandType;
    }
    
    parseMessage(commandValue);
}

/**
 * Разбор и обработка полученной команды
 * @param commandType Тип команды для выполнения
 */
void parseMessage(uint8_t commandType) {
    // Обработать команду
    switch (commandType) {
        case MSG_WATER_PLANTS:
            debugPrintln("Команда: Полив растений");
            waterPlants();
            break;
            
        case MSG_SET_REFERENCE:
            debugPrintln("Команда: Задать эталонное значение влажности");
            // Передаем данные для обработки значения
            setReference(rx_data, rx_length);
            break;
            
        case MSG_SEND_WEATHER:
            debugPrintln("Команда: Отправить погоду");
            sendMoisture();
            break;
            
        default:
            debugPrint("Неизвестная команда: ");
            Serial.println(commandType);
            break;
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
    sendMessage(DEFAULT_MESSAGE);
}

/**
 * Отправка данных о влажности почвы
 */
void sendMoisture() {
    // Передать влажность почвы
    debugPrintln("Отправка данных о влажности...");
    
    // TODO: Заменить на реальное значение от сенсора влажности почвы
    unsigned int sensorValue = getValue();
    value = sensorValue;  // Сохраняем в глобальную переменную
    
    char moistureStr[30];
    sprintf(moistureStr, "Moisture: %d%%", (int)value);
    sendMessage(moistureStr);
}

/**
 * Отправка общего статуса системы полива
 */
void sendStatus() {
    // Передать статус полива
    debugPrintln("Отправка статуса полива...");
    
    // TODO: Добавить дополнительную информацию о состоянии системы полива
    // Формируем строку статуса с текущими значениями
    char statusStr[100];
    sprintf(statusStr, "Status: Mode=%s, Moisture=%d%%, Reference=%d%%, RainSoon=%s", 
            mode == OFF ? "OFF" : (mode == AUTO ? "AUTO" : "ON"),
            (int)value,
            (int)reference, 
            rainSoon ? "Yes" : "No");
    
    sendMessage(statusStr);
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
    
    // Периодическая отправка данных
    static unsigned long lastSendTime = 0;
    unsigned long currentTime = millis();
    
    if (currentTime - lastSendTime > SEND_INTERVAL) {
        sendXBee();
        lastSendTime = currentTime;
    }
    
    // Получение значения влажности
    value = getValue();
    
    // Проверка необходимости полива
    if (mode == AUTO && shouldWater()) {
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