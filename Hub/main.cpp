#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

#define TOGGLE_PIN 3

// EEPROM Configuration
const uint32_t CONFIG_MAGIC_NUMBER = 0x1A2B3C4D;
const int EEPROM_CONFIG_START_ADDRESS = 0;

// Network Configuration
const byte DNS_PORT = 53;


struct ConfigData {
  char WIFI_SSID[64];
  char WIFI_PASSWORD[64];
  char CONFIG_WQTT_TOKEN[64];
  float weather_latitude;
  float weather_longitude;
  char deviceID[16];
  char wqtt_mqtt_host[64];
  int wqtt_mqtt_port;
  char wqtt_mqtt_user[64];
  char wqtt_mqtt_pass[64];
  uint32_t lastSuccessfulWeatherUpdateTimestamp;
  uint32_t magicNumber;
};

const int EEPROM_SIZE = sizeof(ConfigData);

char WIFI_SSID[64] = "";
char WIFI_PASSWORD[64] = "";

char MQTT_HOST[64] = "";
int MQTT_PORT = 0;
char MQTT_USER[64] = "";
char MQTT_PASS[64] = "";
char MQTT_TOPIC_DO[128] = "";
char MQTT_TOPIC_REF[128] = "";
char MQTT_TOPIC_VAL[128] = "";

char CONFIG_WQTT_TOKEN[64] = "";
float weather_latitude = 0.0f;
float weather_longitude = 0.0f;
uint32_t lastSuccessfulWeatherUpdateTimestamp = 0;

char deviceID[16] = "";
bool rainSoon = false;
float hourlyRainForecast[24];
bool wqtt_provisioning_attempted_this_session = false;

WebServer server(80);
DNSServer dnsServer;
bool serverModeActive = false;

const char HTML_CONFIG_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>Настройка Умного Полива</title>
  <style>
    body { font-family: Arial, sans-serif; margin: 20px; background-color: #f4f4f4; color: #333; display: flex; justify-content: center; align-items: center; min-height: 90vh; }
    .container { background-color: #fff; padding: 30px; border-radius: 8px; box-shadow: 0 0 15px rgba(0,0,0,0.2); width: 100%; max-width: 400px; }
    h2 { color: #0056b3; text-align: center; margin-bottom: 20px;}
    label { display: block; margin-bottom: 8px; font-weight: bold; }
    input[type="text"], input[type="password"], input[type="number"] {
      width: calc(100% - 22px); padding: 10px; margin-bottom: 15px; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box;
    }
    button { background-color: #5cb85c; color: white; padding: 10px 15px; border: none; border-radius: 4px; cursor: pointer; font-size: 14px; margin-bottom:15px; width:100%; }
    button:hover { background-color: #4cae4c; }
    input[type="submit"] {
      background-color: #0056b3; color: white; padding: 12px 20px; border: none; border-radius: 4px; cursor: pointer; font-size: 16px; width: 100%;
    }
    input[type="submit"]:hover { background-color: #004494; }
    .message { padding: 10px; margin-top: 20px; margin-bottom: 0px; border-radius: 4px; text-align: center;}
    .success { background-color: #d4edda; color: #155724; border: 1px solid #c3e6cb; }
    .error { background-color: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }
    a { color: #0056b3; text-decoration: none; }
    a:hover { text-decoration: underline; }
    .footer-link { text-align: center; margin-top: 15px; }
    #location_status { font-size: 0.9em; margin-bottom: 10px; min-height:1.2em; }
  </style>
</head>
<body>
  <div class="container">
    <h2>Настройки Умного Полива</h2>
    <form method="POST" action="/save">
      <label for="ssid">WiFi SSID:</label>
      <input type="text" id="ssid" name="ssid" value="%SSID_VAL%">

      <label for="password">WiFi Пароль:</label>
      <input type="password" id="password" name="password">

      <label for="mqtt_token">WQTT API Token:</label>
      <input type="text" id="mqtt_token" name="mqtt_token" value="%WQTT_TOKEN_VAL%">

      <hr style="margin: 20px 0;">
      <p id="location_status">&nbsp;</p>
      <button type="button" onclick="getLocation()">Получить текущие координаты</button>

      <label for="latitude">Широта (Latitude):</label>
      <input type="number" step="any" id="latitude" name="latitude" value="%LATITUDE_VAL%" required>

      <label for="longitude">Долгота (Longitude):</label>
      <input type="number" step="any" id="longitude" name="longitude" value="%LONGITUDE_VAL%" required>
      <hr style="margin: 20px 0;">

      <input type="submit" value="Сохранить">
    </form>
    <button type="button" onclick="confirmReset()" style="background-color: #d9534f; margin-top: 10px;">Сбросить настройки до заводских</button>
    <div id="message-container" style="margin-top: 20px;"></div>
  </div>
  <script>
    function getLocation() {
      var statusP = document.getElementById("location_status");
      if (navigator.geolocation) {
        statusP.innerHTML = "Запрос координат...";
        navigator.geolocation.getCurrentPosition(showPosition, showError, {enableHighAccuracy:true, timeout:5000, maximumAge:0});
      } else { 
        statusP.innerHTML = "Геолокация не поддерживается вашим браузером.";
      }
    }

    function showPosition(position) {
      document.getElementById('latitude').value = position.coords.latitude.toFixed(6);
      document.getElementById('longitude').value = position.coords.longitude.toFixed(6);
      document.getElementById("location_status").innerHTML = "Координаты получены!";
    }

    function showError(error) {
      var statusP = document.getElementById("location_status");
      switch(error.code) {
        case error.PERMISSION_DENIED:
          statusP.innerHTML = "Запрос геолокации отклонен."
          break;
        case error.POSITION_UNAVAILABLE:
          statusP.innerHTML = "Информация о местоположении недоступна."
          break;
        case error.TIMEOUT:
          statusP.innerHTML = "Тайм-аут запроса геолокации."
          break;
        case error.UNKNOWN_ERROR:
          statusP.innerHTML = "Неизвестная ошибка геолокации."
          break;
      }
    }

    function confirmReset() {
      var messageDiv = document.getElementById('message-container');
      messageDiv.innerHTML = '';
      if (confirm("Вы уверены, что хотите сбросить все настройки до заводских? Это действие необратимо и приведет к перезагрузке устройства.")) {
        messageDiv.innerHTML = '<p>Выполняется сброс...</p>';
        fetch('/reset', { method: 'POST' })
          .then(response => {
            if (!response.ok) {
              throw new Error('Network response was not ok: ' + response.statusText);
            }
            return response.text();
          })
          .then(data => {
            messageDiv.innerHTML = '<p class="success">' + data + '</p>';
          })
          .catch(error => {
            console.error('Error during reset:', error);
            messageDiv.innerHTML = '<p class="error">Ошибка при сбросе настроек: ' + error.message + '</p>';
          });
      }
    }
  </script>
</body>
</html>
)rawliteral";

/**
 * @brief Обрабатывает запросы плейсхолдеров в HTML-шаблоне.
 * @param var Имя плейсхолдера.
 * @return Соответствующее значение для плейсхолдера или пустая строка, если плейсхолдер не найден.
 */
String processor(const String& var) {
  if (var == "SSID_VAL") return WIFI_SSID;
  if (var == "WQTT_TOKEN_VAL") return CONFIG_WQTT_TOKEN;
  if (var == "LATITUDE_VAL") return String(weather_latitude, 2);
  if (var == "LONGITUDE_VAL") return String(weather_longitude, 2);
  return String();
}

/**
 * @brief Обрабатывает запросы к несуществующим страницам.
 * @details Перенаправляет на IP-адрес точки доступа, если запрос пришел не на него,
 * в противном случае возвращает ошибку 404.
 */
void handleNotFound() {
  String host = server.hostHeader();
  if (!host.equals(WiFi.softAPIP().toString())) {
      server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
      server.send(302, "text/plain", ""); 
      return;
  }
  server.send(404, "text/plain", "404: Страница не найдена");
}

/* Сервер */

/**
 * @brief Настраивает и запускает веб-сервер в режиме точки доступа для конфигурации устройства.
 * @details Создает точку доступа Wi-Fi, запускает DNS-сервер для перехвата всех запросов
 * и HTTP-сервер для отображения страницы настроек.
 */
void setupServer() {
    Serial.println("Активация режима точки доступа для конфигурации...");
    WiFi.mode(WIFI_AP);
    IPAddress apIP(192, 168, 4, 1);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP("SmartWatering-Config", "password");

    Serial.print("IP адрес точки доступа: http://");
    Serial.println(WiFi.softAPIP());

    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(DNS_PORT, "*", apIP);
    Serial.println("DNS сервер запущен.");

    server.on("/", HTTP_GET, handle_get);
    server.on("/save", HTTP_POST, handle_post);
    server.on("/reset", HTTP_POST, handle_reset);
    server.onNotFound(handleNotFound);
  
    server.begin();
    Serial.println("HTTP сервер запущен в режиме конфигурации.");
    serverModeActive = true;
}

/**
 * @brief Обрабатывает GET-запросы на корневой URL ("/").
 * @details Отправляет клиенту HTML-страницу конфигурации с подставленными текущими значениями.
 */
void handle_get() {
    String content = FPSTR(HTML_CONFIG_PAGE);
    content.replace("%SSID_VAL%", processor("SSID_VAL"));
    content.replace("%WQTT_TOKEN_VAL%", processor("WQTT_TOKEN_VAL"));
    content.replace("%LATITUDE_VAL%", processor("LATITUDE_VAL"));
    content.replace("%LONGITUDE_VAL%", processor("LONGITUDE_VAL"));
    server.send(200, "text/html", content);
}

/**
 * @brief Обрабатывает POST-запросы на URL "/save" для сохранения настроек.
 * @details Считывает переданные параметры (SSID, пароль, токен, координаты),
 * сохраняет их в глобальные переменные, вызывает saveConfig() и перезагружает устройство.
 */
void handle_post() {
    if (server.hasArg("ssid")) {
        strncpy(WIFI_SSID, server.arg("ssid").c_str(), sizeof(WIFI_SSID) - 1);
        WIFI_SSID[sizeof(WIFI_SSID) - 1] = '\0';
    }
    if (server.hasArg("password") && server.arg("password").length() > 0) {
        strncpy(WIFI_PASSWORD, server.arg("password").c_str(), sizeof(WIFI_PASSWORD) - 1);
        WIFI_PASSWORD[sizeof(WIFI_PASSWORD) - 1] = '\0';
    }
    if (server.hasArg("mqtt_token")) {
        strncpy(CONFIG_WQTT_TOKEN, server.arg("mqtt_token").c_str(), sizeof(CONFIG_WQTT_TOKEN) - 1);
        CONFIG_WQTT_TOKEN[sizeof(CONFIG_WQTT_TOKEN) - 1] = '\0';
    }
    if (server.hasArg("latitude")) {
        weather_latitude = server.arg("latitude").toFloat();
    }
    if (server.hasArg("longitude")) {
        weather_longitude = server.arg("longitude").toFloat();
    }

    Serial.println("Настройки получены веб-сервером:");
    Serial.print("SSID: "); Serial.println(WIFI_SSID);
    Serial.print("WQTT API Token: "); Serial.println(CONFIG_WQTT_TOKEN);
    Serial.print("Latitude: "); Serial.println(weather_latitude, 2);
    Serial.print("Longitude: "); Serial.println(weather_longitude, 2);
  
    saveConfig();

    Serial.println("Перезагрузка через 2 секунды...");
    delay(2000);
    ESP.restart();
}

/**
 * @brief Обрабатывает POST-запросы на URL "/reset" для сброса настроек к заводским.
 * @details Очищает область EEPROM, выделенную для настроек, и перезагружает устройство.
 */
void handle_reset() {
    Serial.println("Получен запрос на сброс настроек до заводских.");
    EEPROM.begin(EEPROM_SIZE);
    for (int i = 0; i < EEPROM_SIZE; i++) {
        EEPROM.write(i, 0xFF);
    }
    bool success = EEPROM.commit();
    EEPROM.end();

    if (success) {
        Serial.println("EEPROM успешно очищен.");
        server.send(200, "text/plain", "Настройки успешно сброшены. Перезагрузка через 3 секунды...");
    } else {
        Serial.println("Ошибка очистки EEPROM.");
        server.send(500, "text/plain", "Ошибка при сбросе настроек.");
    }
    delay(3000);
    ESP.restart();
}

/**
 * @brief Регистрирует устройство на платформе WQTT.
 * @details Отправляет POST-запрос на /api/devices с данными об устройстве (имя, тип, датчики).
 * В случае успеха сохраняет полученный deviceID.
 * @return true, если устройство успешно зарегистрировано, иначе false.
 */
bool registerDevice() {
    WiFiClientSecure clientSecure;
    HTTPClient http;
    bool success = false;

    Serial.println("Регистрация устройства на WQTT (/api/devices)...");
    DynamicJsonDocument docRequest(512);
    docRequest["name"] = "Полив";
    docRequest["type"] = 19;
    docRequest["room"] = "Сад";
  
    JsonArray sensors_float = docRequest.createNestedArray("sensors_float");
    JsonObject sensor1 = sensors_float.createNestedObject();
    sensor1["type"] = 1;
    sensor1["topic"] = "humidity/sensor";
    sensor1["multiplier"] = 1;

    JsonArray on_off = docRequest.createNestedArray("on_off");
    JsonObject on_off1 = on_off.createNestedObject();
    on_off1["topic_cmd"] = "irrigation/command";
    on_off1["topic_state"] = "";
    on_off1["cmd_on"] = "1";
    on_off1["cmd_off"] = "0";

    JsonArray range = docRequest.createNestedArray("range");
    JsonObject range1 = range.createNestedObject();
    range1["type"] = 2;
    range1["topic_cmd"] = "humidity/set";
    range1["topic_state"] = "";
    range1["max"] = 100;
    range1["min"] = 0;
    range1["precision"] = 1;
    range1["multiplier"] = 1;

    String requestBody;
    serializeJson(docRequest, requestBody);
    Serial.print("Тело запроса на создание устройства WQTT: ");
    Serial.println(requestBody);

    String authHeader = "Token " + String(CONFIG_WQTT_TOKEN);

    clientSecure.setInsecure();
    if (http.begin(clientSecure, "https://dash.wqtt.ru/api/devices")) {
        http.addHeader("Content-Type", "application/json");
        http.addHeader("Authorization", authHeader);
        http.addHeader("Accept", "*/*");
    
        int httpCode = http.POST(requestBody);
        Serial.print("HTTP POST /api/devices, код ответа: "); Serial.println(httpCode);
        if (httpCode == 200 || httpCode == 201) {
            String payload = http.getString();
            Serial.print("Ответ от WQTT (создание устройства): "); Serial.println(payload);
            DynamicJsonDocument docResponse(256);
            DeserializationError error = deserializeJson(docResponse, payload);
            if (!error && docResponse["result"] == "ok" && docResponse["detail"].containsKey("device_id")) {
                long newDeviceID_long = docResponse["detail"]["device_id"];
                sprintf(deviceID, "%ld", newDeviceID_long);
                Serial.print("Устройство успешно создано. Device ID: "); Serial.println(deviceID);
                success = true;
            } else { 
                Serial.println("Ошибка в ответе WQTT при создании устройства или парсинге JSON.");
                if(error) Serial.println(error.c_str());
            }
        } else { 
            Serial.print("Ошибка HTTP POST /api/devices: "); Serial.println(http.errorToString(httpCode).c_str());
        }
        http.end();
    } else { 
        Serial.println("Не удалось подключиться к /api/devices");
    }
    return success;
}

/**
 * @brief Получает данные MQTT-брокера с платформы WQTT.
 * @details Отправляет GET-запрос на /api/broker, используя токен авторизации.
 * В случае успеха сохраняет хост, порт, имя пользователя и пароль MQTT-брокера.
 * @return true, если данные брокера успешно получены, иначе false.
 */
bool fetchMqttBrokerDetailsFromWqtt() {
    WiFiClientSecure clientSecure;
    HTTPClient http;
    bool success = false;
    String authHeader = "Token " + String(CONFIG_WQTT_TOKEN);

    Serial.println("Получение данных MQTT брокера от WQTT (/api/broker)...");
    clientSecure.setInsecure();
    if (http.begin(clientSecure, "https://dash.wqtt.ru/api/broker")) {
        http.addHeader("Authorization", authHeader);
        http.addHeader("Accept", "*/*");
        int httpCode = http.GET();
        Serial.print("HTTP GET /api/broker, код ответа: "); Serial.println(httpCode);
        if (httpCode == 200) {
            String payload = http.getString();
            Serial.print("Ответ от WQTT (данные брокера): "); Serial.println(payload);
            DynamicJsonDocument docResponse(512);
            DeserializationError error = deserializeJson(docResponse, payload);
            if (!error) {
                strncpy(MQTT_HOST, docResponse["server"] | "", sizeof(MQTT_HOST) - 1);
                MQTT_HOST[sizeof(MQTT_HOST)-1] = '\0';
                MQTT_PORT = docResponse["port"] | 0;
                strncpy(MQTT_USER, docResponse["user"] | "", sizeof(MQTT_USER) - 1);
                MQTT_USER[sizeof(MQTT_USER)-1] = '\0';
                strncpy(MQTT_PASS, docResponse["password"] | "", sizeof(MQTT_PASS) - 1);
                MQTT_PASS[sizeof(MQTT_PASS)-1] = '\0';
                Serial.println("Данные MQTT брокера получены.");
                Serial.print("Host: "); Serial.println(MQTT_HOST);
                Serial.print("Port: "); Serial.println(MQTT_PORT);
                Serial.print("User: "); Serial.println(MQTT_USER);
                success = true;
            } else { 
                Serial.print("Ошибка парсинга JSON ответа (данные брокера): "); Serial.println(error.c_str());
            }
        } else { 
            Serial.print("Ошибка HTTP GET /api/broker: "); Serial.println(http.errorToString(httpCode).c_str());
        }
        http.end();
    } else { 
        Serial.println("Не удалось подключиться к /api/broker");
    }
    return success;
}

/**
 * @brief Обновляет прогноз погоды, если это необходимо.
 * @details Проверяет, прошло ли более 24 часов с момента последнего успешного обновления или прогноз отсутствует.
 * В случае необходимости запрашивает данные о почасовом количестве осадков с open-meteo.com.
 * Выполняет до MAX_WEATHER_ATTEMPTS попыток с задержкой WEATHER_RETRY_DELAY_MS.
 * Сохраняет прогноз и время обновления в EEPROM через saveConfig().
 * @return true, если прогноз успешно обновлен или уже актуален, иначе false.
 */
bool updateWeatherForecastIfNeeded() {
    if (weather_latitude == 0.0f && weather_longitude == 0.0f) {
        Serial.println("Координаты для прогноза погоды не установлены. Обновление невозможно.");
        return false;
    }

    Serial.println("Проверка необходимости обновления прогноза погоды...");
    uint32_t currentUnixTime = getCurrentUnixTime();

    if (currentUnixTime == 0) {
        Serial.println("Не удалось получить текущее время. Обновление прогноза погоды отложено.");
        return false;
    }

    const uint32_t TWENTY_FOUR_HOURS_SECONDS = 24 * 60 * 60;
    if ((currentUnixTime - lastSuccessfulWeatherUpdateTimestamp > TWENTY_FOUR_HOURS_SECONDS) || lastSuccessfulWeatherUpdateTimestamp == 0) {
        Serial.println("Требуется обновление прогноза погоды (прошло >24ч или нет данных).");
        
        const int MAX_WEATHER_ATTEMPTS = 3;
        const int WEATHER_RETRY_DELAY_MS = 5000;
        bool weatherUpdatedSuccessfully = false;

        for (int attempt = 1; attempt <= MAX_WEATHER_ATTEMPTS; ++attempt) {
            Serial.print("Попытка обновления погоды "); Serial.print(attempt); Serial.print("/"); Serial.println(MAX_WEATHER_ATTEMPTS);
            WiFiClientSecure clientSecure;
            HTTPClient http;
            String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(weather_latitude, 2) +
                       "&longitude=" + String(weather_longitude, 2) + "&hourly=rain&forecast_days=1";
            Serial.print("Weather API URL: "); Serial.println(url);

            clientSecure.setInsecure();
            if (http.begin(clientSecure, url)) {
                int httpCode = http.GET();
                Serial.print("Прогноз погоды, HTTP GET, код ответа: "); Serial.println(httpCode);
                if (httpCode == HTTP_CODE_OK) {
                    String payload = http.getString();
                    Serial.println("Ответ от Open-Meteo:"); Serial.println(payload);
                    DynamicJsonDocument doc(2048);
                    DeserializationError error = deserializeJson(doc, payload);
                    if (!error && doc.containsKey("hourly") && doc["hourly"].containsKey("rain")) {
                        JsonArray hourly_rain = doc["hourly"]["rain"].as<JsonArray>();
                        int i = 0;
                        bool anyRainThisForecast = false;
                        for (JsonVariant v : hourly_rain) {
                            if (i < 24) {
                                hourlyRainForecast[i] = v.as<float>();
                                if (hourlyRainForecast[i] > 0.0f) anyRainThisForecast = true;
                                Serial.print("Час "); Serial.print(i); Serial.print(": "); Serial.print(hourlyRainForecast[i]); Serial.println("mm");
                                i++;
                            }
                        }
                        for (; i < 24; ++i) hourlyRainForecast[i] = 0.0f;
                        rainSoon = anyRainThisForecast;
                        lastSuccessfulWeatherUpdateTimestamp = currentUnixTime;
                        saveConfig();
                        Serial.println(rainSoon ? "Прогноз: Ожидается дождь." : "Прогноз: Дождя не ожидается.");
                        Serial.println("Прогноз погоды успешно обновлен и сохранен.");
                        weatherUpdatedSuccessfully = true;
                        http.end();
                        break;
                    } else {
                        Serial.print("Ошибка парсинга JSON погоды: ");
                        if(error) Serial.println(error.c_str());
                        else Serial.println("Отсутствуют необходимые поля в JSON.");
                    }
                } else {
                    Serial.print("Ошибка HTTP GET прогноза погоды: "); Serial.println(http.errorToString(httpCode).c_str());
                }
                http.end();
            } else {
                Serial.println("Не удалось подключиться к API погоды.");
            }

            if (!weatherUpdatedSuccessfully && attempt < MAX_WEATHER_ATTEMPTS) {
                Serial.print("Следующая попытка обновления погоды через "); Serial.print(WEATHER_RETRY_DELAY_MS / 1000); Serial.println(" сек...");
                delay(WEATHER_RETRY_DELAY_MS);
            }
        }

        if (!weatherUpdatedSuccessfully) {
            Serial.println("Не удалось обновить прогноз погоды после всех попыток.");
            return false;
        }
        return true;
    } else {
        Serial.println("Прогноз погоды актуален. Используется кэш.");
        return true;
    }
}

/**
 * @brief Получает текущее время Unix timestamp с aisenseapi.com.
 * @details Выполняет до MAX_ATTEMPTS (3) попыток с задержкой RETRY_DELAY_MS (2 секунды) между ними.
 * @return Текущий Unix timestamp в случае успеха, иначе 0.
 */
uint32_t getCurrentUnixTime() {
    const int MAX_ATTEMPTS = 3;
    const int RETRY_DELAY_MS = 2000;
    WiFiClientSecure clientSecure;
    HTTPClient http;

    Serial.println("Получение текущего Unix времени...");

    for (int attempt = 1; attempt <= MAX_ATTEMPTS; ++attempt) {
        Serial.print("Попытка "); Serial.print(attempt); Serial.print("/"); Serial.println(MAX_ATTEMPTS);
        clientSecure.setInsecure();
        if (http.begin(clientSecure, "https://aisenseapi.com/services/v1/timestamp")) {
            int httpCode = http.GET();
            if (httpCode == HTTP_CODE_OK) {
                String payload = http.getString();
                Serial.print("Ответ от aisenseapi.com: "); Serial.println(payload);
                DynamicJsonDocument doc(256);
                DeserializationError error = deserializeJson(doc, payload);
                if (!error && doc.containsKey("timestamp")) {
                    uint32_t unixTime = doc["timestamp"].as<uint32_t>();
                    Serial.print("Текущее Unix время: "); Serial.println(unixTime);
                    http.end();
                    return unixTime;
                } else {
                    Serial.print("Ошибка парсинга JSON времени");
                    if (error) Serial.println(error.c_str());
                    else Serial.println("Отсутствует поле 'timestamp' или ошибка парсинга.");
                }
            } else {
                Serial.print("Ошибка HTTP GET для времени: "); 
                Serial.println(http.errorToString(httpCode).c_str());
            }
            http.end();
        } else {
            Serial.println("Не удалось подключиться к aisenseapi.com");
        }
        if (attempt < MAX_ATTEMPTS) {
            Serial.print("Следующая попытка через "); Serial.print(RETRY_DELAY_MS / 1000); Serial.println(" сек...");
            delay(RETRY_DELAY_MS);
        }
    }
    Serial.println("Не удалось получить текущее время после всех попыток.");
    return 0;
}

/**
 * @brief Сообщает, ожидается ли дождь сегодня, предварительно обновив прогноз, если это необходимо.
 * @return true, если ожидается дождь и прогноз актуален, иначе false.
 */
bool getWeather() {
    Serial.println("Запрос прогноза дождя...");
    if (updateWeatherForecastIfNeeded()) {
        if (rainSoon) {
            Serial.println("Прогноз: Ожидается дождь сегодня.");
            return true;
        } else {
            Serial.println("Прогноз: Дождя сегодня не ожидается.");
            return false;
        }
    } else {
        Serial.println("Не удалось получить актуальный прогноз погоды.");
        return false;
    }
}

/* Настройки */

/**
 * @brief Сохраняет текущую конфигурацию устройства в EEPROM.
 * @details Сохраняет SSID и пароль Wi-Fi, токен WQTT, координаты, deviceID,
 * данные MQTT-брокера и время последнего обновления погоды.
 * Также сохраняет "магическое число" для проверки корректности данных при загрузке.
 */
void saveConfig() {
    Serial.println("Сохранение конфигурации в EEPROM...");
    ConfigData configToSave;

    strncpy(configToSave.WIFI_SSID, WIFI_SSID, sizeof(configToSave.WIFI_SSID) - 1);
    configToSave.WIFI_SSID[sizeof(configToSave.WIFI_SSID) - 1] = '\0';
    strncpy(configToSave.WIFI_PASSWORD, WIFI_PASSWORD, sizeof(configToSave.WIFI_PASSWORD) - 1);
    configToSave.WIFI_PASSWORD[sizeof(configToSave.WIFI_PASSWORD) - 1] = '\0';
    strncpy(configToSave.CONFIG_WQTT_TOKEN, CONFIG_WQTT_TOKEN, sizeof(configToSave.CONFIG_WQTT_TOKEN) - 1);
    configToSave.CONFIG_WQTT_TOKEN[sizeof(configToSave.CONFIG_WQTT_TOKEN) - 1] = '\0';
    configToSave.weather_latitude = weather_latitude;
    configToSave.weather_longitude = weather_longitude;
    strncpy(configToSave.deviceID, deviceID, sizeof(configToSave.deviceID) - 1);
    configToSave.deviceID[sizeof(configToSave.deviceID) - 1] = '\0';
    strncpy(configToSave.wqtt_mqtt_host, MQTT_HOST, sizeof(configToSave.wqtt_mqtt_host) - 1);
    configToSave.wqtt_mqtt_host[sizeof(configToSave.wqtt_mqtt_host) - 1] = '\0';
    configToSave.wqtt_mqtt_port = MQTT_PORT;
    strncpy(configToSave.wqtt_mqtt_user, MQTT_USER, sizeof(configToSave.wqtt_mqtt_user) - 1);
    configToSave.wqtt_mqtt_user[sizeof(configToSave.wqtt_mqtt_user) - 1] = '\0';
    strncpy(configToSave.wqtt_mqtt_pass, MQTT_PASS, sizeof(configToSave.wqtt_mqtt_pass) - 1);
    configToSave.wqtt_mqtt_pass[sizeof(configToSave.wqtt_mqtt_pass) - 1] = '\0';
    configToSave.lastSuccessfulWeatherUpdateTimestamp = lastSuccessfulWeatherUpdateTimestamp;
    configToSave.magicNumber = CONFIG_MAGIC_NUMBER;

    EEPROM.begin(EEPROM_SIZE);
    EEPROM.put(EEPROM_CONFIG_START_ADDRESS, configToSave);
    if (EEPROM.commit()) {
        Serial.println("Конфигурация успешно сохранена в EEPROM.");
    } else {
        Serial.println("Ошибка: не удалось сохранить конфигурацию в EEPROM.");
    }
    EEPROM.end();
}

/**
 * @brief Загружает конфигурацию устройства из EEPROM.
 * @details Если в EEPROM найдена корректная конфигурация (проверяется по "магическому числу"),
 * загружает SSID и пароль Wi-Fi, токен WQTT, координаты, deviceID,
 * данные MQTT-брокера и время последнего обновления погоды в глобальные переменные.
 * В противном случае инициализирует переменные значениями по умолчанию.
 */
void loadConfig() {
    Serial.println("Загрузка конфигурации из EEPROM...");
    ConfigData loadedConfig;
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.get(EEPROM_CONFIG_START_ADDRESS, loadedConfig);
    EEPROM.end();

    if (loadedConfig.magicNumber == CONFIG_MAGIC_NUMBER) {
        Serial.println("Обнаружена корректная конфигурация в EEPROM.");
        strncpy(WIFI_SSID, loadedConfig.WIFI_SSID, sizeof(WIFI_SSID) - 1);
        WIFI_SSID[sizeof(WIFI_SSID) - 1] = '\0';
        strncpy(WIFI_PASSWORD, loadedConfig.WIFI_PASSWORD, sizeof(WIFI_PASSWORD) - 1);
        WIFI_PASSWORD[sizeof(WIFI_PASSWORD) - 1] = '\0';
        strncpy(CONFIG_WQTT_TOKEN, loadedConfig.CONFIG_WQTT_TOKEN, sizeof(CONFIG_WQTT_TOKEN) - 1);
        CONFIG_WQTT_TOKEN[sizeof(CONFIG_WQTT_TOKEN) - 1] = '\0';
        weather_latitude = loadedConfig.weather_latitude;
        weather_longitude = loadedConfig.weather_longitude;
        strncpy(deviceID, loadedConfig.deviceID, sizeof(deviceID) - 1);
        deviceID[sizeof(deviceID) - 1] = '\0';
        strncpy(MQTT_HOST, loadedConfig.wqtt_mqtt_host, sizeof(MQTT_HOST) - 1);
        MQTT_HOST[sizeof(MQTT_HOST)-1] = '\0';
        MQTT_PORT = loadedConfig.wqtt_mqtt_port;
        strncpy(MQTT_USER, loadedConfig.wqtt_mqtt_user, sizeof(MQTT_USER) - 1);
        MQTT_USER[sizeof(MQTT_USER)-1] = '\0';
        strncpy(MQTT_PASS, loadedConfig.wqtt_mqtt_pass, sizeof(MQTT_PASS) - 1);
        MQTT_PASS[sizeof(MQTT_PASS)-1] = '\0';
        lastSuccessfulWeatherUpdateTimestamp = loadedConfig.lastSuccessfulWeatherUpdateTimestamp;

        Serial.println("Конфигурация загружена:");
        Serial.print("SSID: "); Serial.println(WIFI_SSID);
        Serial.print("Password: "); Serial.println(WIFI_PASSWORD);
        Serial.print("WQTT API Token: "); Serial.println(CONFIG_WQTT_TOKEN);
        Serial.print("Latitude: "); Serial.println(weather_latitude, 2);
        Serial.print("Longitude: "); Serial.println(weather_longitude, 2);
        Serial.print("Device ID: "); Serial.println(deviceID);
        Serial.print("MQTT Host: "); Serial.println(MQTT_HOST);
        Serial.print("MQTT Port: "); Serial.println(MQTT_PORT);
        Serial.print("Last Successful Weather Update Timestamp: "); Serial.println(lastSuccessfulWeatherUpdateTimestamp);
    } else {
        Serial.println("Корректная конфигурация в EEPROM не найдена. Инициализация значений по умолчанию.");
        WIFI_SSID[0] = '\0';
        WIFI_PASSWORD[0] = '\0'; 
        CONFIG_WQTT_TOKEN[0] = '\0';
        weather_latitude = 0.0f;
        weather_longitude = 0.0f;
        deviceID[0] = '\0';
        MQTT_HOST[0] = '\0';
        MQTT_PORT = 0;
        MQTT_USER[0] = '\0';
        MQTT_PASS[0] = '\0';
        lastSuccessfulWeatherUpdateTimestamp = 0;
    }
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

/**
 * @brief Инициализирует базовые системы: Serial порт и массив прогноза погоды.
 */
void initializeBaseSystems() {
    Serial.begin(115200);
    for (int i = 0; i < 24; ++i) hourlyRainForecast[i] = -1.0f;
    Serial.println("\n\nЗагрузка контроллера Умного Полива...");
}

/**
 * @brief Проверяет, нужно ли принудительно войти в режим конфигурации по состоянию TOGGLE_PIN.
 * @return true, если нужно войти в режим конфигурации, иначе false.
 */
bool checkForceConfigMode() {
    pinMode(TOGGLE_PIN, INPUT_PULLUP);
    delay(100); 
    bool forceConfig = (digitalRead(TOGGLE_PIN) == LOW);
    if (forceConfig) {
        Serial.println("Кнопка конфигурации нажата. Принудительный вход в режим конфигурации.");
    }
    return forceConfig;
}

/**
 * @brief Выполняет задачи, необходимые после успешного подключения к Wi-Fi.
 */
void handleWiFiConnectedTasks() {
    Serial.println("\nWiFi подключен!");
    Serial.print("IP адрес: "); Serial.println(WiFi.localIP());
    
    bool configChanged = false;
            
    if (strlen(CONFIG_WQTT_TOKEN) > 0) {
        if (strlen(deviceID) == 0 && !wqtt_provisioning_attempted_this_session) {
            Serial.println("Device ID не найден. Попытка регистрации на WQTT...");
            wqtt_provisioning_attempted_this_session = true;
            if (registerDevice()) {
                Serial.println("Устройство WQTT успешно зарегистрировано. Device ID: " + String(deviceID));
                configChanged = true; 
            } else {
                Serial.println("Не удалось зарегистрировать устройство на WQTT в этой сессии.");
            }
        } else if (strlen(deviceID) > 0) {
            Serial.println("Device ID уже существует.");
        }

        if (strlen(deviceID) > 0 && strlen(MQTT_HOST) == 0) { 
            Serial.println("Device ID есть, но нет данных MQTT брокера. Попытка получения...");
            if (fetchMqttBrokerDetailsFromWqtt()) {
                Serial.println("Данные MQTT брокера успешно получены от WQTT.");
                configChanged = true;
            } else {
                Serial.println("Не удалось получить данные MQTT брокера от WQTT.");
            }
        } else if (strlen(MQTT_HOST) > 0) {
             Serial.println("Данные MQTT брокера уже загружены.");
        }

        if (configChanged) {
            Serial.println("Обнаружены изменения в конфигурации. Сохранение...");
            saveConfig();
        }

    } else {
        Serial.println("Токен WQTT API не настроен. Регистрация и получение данных MQTT пропущены.");
    }
            
    if (weather_latitude != 0.0f || weather_longitude != 0.0f) {
            updateWeatherForecastIfNeeded(); 
    } else {
        Serial.println("Координаты для прогноза погоды не заданы. Прогноз не будет получен при старте.");
    }

    getWeather();
}

/**
 * @brief Пытается подключиться к Wi-Fi, совершая несколько попыток.
 * @details Выполняет MAX_MAIN_ATTEMPTS основных попыток, каждая из которых включает
 * INNER_ATTEMPTS_MAX внутренних проверок статуса подключения.
 * @return true, если подключение успешно, иначе false.
 */
bool attemptWiFiConnection() {
    Serial.println("Попытка подключения к WiFi...");
    WiFi.mode(WIFI_STA);

    const int MAX_MAIN_ATTEMPTS = 3;
    const int DELAY_BETWEEN_MAIN_ATTEMPTS_MS = 5000;
    const int INNER_ATTEMPTS_MAX = 30; 
    const int INNER_ATTEMPT_DELAY_MS = 500;

    for (int mainAttemptNum = 1; mainAttemptNum <= MAX_MAIN_ATTEMPTS; ++mainAttemptNum) {
        Serial.print("Попытка подключения (");
        Serial.print(mainAttemptNum);
        Serial.print("/");
        Serial.print(MAX_MAIN_ATTEMPTS);
        Serial.print(") к ");
        Serial.println(WIFI_SSID);

        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        
        int innerAttemptsCount = 0;
        while (WiFi.status() != WL_CONNECTED && innerAttemptsCount < INNER_ATTEMPTS_MAX) { 
            delay(INNER_ATTEMPT_DELAY_MS); 
            Serial.print("."); 
            innerAttemptsCount++;
        }
        Serial.println(); 

        if (WiFi.status() == WL_CONNECTED) {
            return true; 
        } else {
            Serial.print("Попытка ");
            Serial.print(mainAttemptNum);
            Serial.println(" не удалась.");
            if (mainAttemptNum < MAX_MAIN_ATTEMPTS) {
                Serial.print("Следующая попытка через ");
                Serial.print(DELAY_BETWEEN_MAIN_ATTEMPTS_MS / 1000);
                Serial.println(" секунд...");
                delay(DELAY_BETWEEN_MAIN_ATTEMPTS_MS);
            }
        }
    }
    Serial.println("Все попытки подключения к WiFi не удались.");
    return false; 
}


void setup() {
    initializeBaseSystems();
    bool forceConfig = checkForceConfigMode();
    serverModeActive = false;
    loadConfig();

    if (forceConfig || strlen(WIFI_SSID) == 0) {
        Serial.println("Переход в режим конфигурации...");
        setupServer();
    } else {
        if (attemptWiFiConnection()) {
            handleWiFiConnectedTasks();
        } else {
            Serial.println("Переход в режим конфигурации из-за неудачного подключения к WiFi.");
            setupServer();
        }
    }
}

void loop() {
    if (serverModeActive) {
        dnsServer.processNextRequest(); 
        server.handleClient();        
    } else {
        if (WiFi.status() == WL_CONNECTED) {
        } else {
            Serial.println("WiFi соединение потеряно в рабочем режиме.");
            attemptWiFiConnection();
            delay(5000);
        }
    }
}