#include <Arduino.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <XBee.h>

#include <Constants.h>
#include <Pages.h>

Config config;

WiFiClient wifiClient;
WiFiClientSecure clientSecure;

WebServer webServer;
HTTPClient httpClient;

PubSubClient mqttClient(wifiClient);
XBeeWithCallbacks xbeeClient;

bool serverMode;
unsigned long weatherLast = -WEATHER_INTERVAL;

/* Настройки */

void loadConfig() {
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.get(0, config);
    EEPROM.end();

    if (config.WIFI_SSID[0] == 0xFF) config.WIFI_SSID[0] = '\0';
    if (config.WIFI_PASSWORD[0] == 0xFF) config.WIFI_PASSWORD[0] = '\0';

    if (config.MQTT_HOST[0] == 0xFF) config.MQTT_HOST[0] = '\0';
    if (config.MQTT_PORT == 0xFF) config.MQTT_PORT = 0;
    if (config.MQTT_USERNAME[0] == 0xFF) config.MQTT_USERNAME[0] = '\0';
    if (config.MQTT_PASSWORD[0] == 0xFF) config.MQTT_PASSWORD[0] = '\0';

    if (config.DEVICE_ID == 0xFF) config.DEVICE_ID = 0;
    if (config.WQTT_TOKEN[0] == 0xFF) config.WQTT_TOKEN[0] = '\0';

    if (config.WEATHER_LATITUDE == 0xFF) config.WEATHER_LATITUDE = 0;
    if (config.WEATHER_LONGITUDE == 0xFF) config.WEATHER_LONGITUDE = 0;

    Serial.println("Config loaded.");
}

void saveConfig() {
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.put(0, config);
    EEPROM.commit();
    EEPROM.end();
}

void resetConfig() {
    EEPROM.begin(EEPROM_SIZE);
    for (int i = 0; i < EEPROM_SIZE; i++) {
        EEPROM.write(i, 0xFF);
    }
    EEPROM.commit();
    EEPROM.end();
}

/* ZigBee */

void zbReceive(ZBRxResponse &rx, unsigned int) {
    const int payloadLength = rx.getDataLength();
    char payload[payloadLength + 1];
    memcpy(payload, rx.getData(), payloadLength);
    payload[payloadLength] = '\0';

    Serial.print("ZigBee payload received: ");
    Serial.println(payload);

    const char *command = strtok(payload, "=");
    const char *value = strtok(nullptr, "=");

    if (strcmp(command, XBEE_COMMAND_VALUE) == 0) {
        mqttClient.publish(MQTT_TOPIC_VALUE, value);
    } else if (strcmp(command,  XBEE_COMMAND_STATUS) == 0) {
        mqttClient.publish(MQTT_TOPIC_STATUS, value);
    } else if (strcmp(command,  XBEE_COMMAND_WATER) == 0) {
        mqttClient.publish(MQTT_TOPIC_WATER, value);
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

/* WiFi */

bool wifiConnect() {
    Serial.print("Connecting to WiFi network...");
    WiFi.begin(config.WIFI_SSID, config.WIFI_PASSWORD);
    for (unsigned int i = 0; i < 10; i++) {
        if (WiFiClass::status() == WL_CONNECTED) {
            Serial.println("connected");
            return true;
        }
        Serial.print(".");
        delay(1000);
    }
    Serial.println("not connected");
    return false;
}

void wifiShare() {
    WiFi.disconnect();
    WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("WiFi host is started on ");
    Serial.println(WiFi.softAPIP());
}

/* API */

int registerDevice() {
    Serial.print("Registering device...");

    JsonDocument docRequest;
    docRequest["name"] = "Полив";
    docRequest["type"] = 19;
    docRequest["room"] = "Сад";
    const JsonArray sensors_float = docRequest["sensors_float"].to<JsonArray>();
    const JsonObject sensor1 = sensors_float.add<JsonObject>();
    sensor1["type"] = 1;
    sensor1["topic"] = "moisture/value";
    sensor1["multiplier"] = 1;
    const JsonObject sensor2 = sensors_float.add<JsonObject>();
    sensor2["type"] = 7;
    sensor2["topic"] = "water/level";
    sensor2["multiplier"] = 1;
    const JsonArray sensors_event = docRequest["sensors_event"].to<JsonArray>();
    const JsonObject sensor = sensors_event.add<JsonObject>();
    sensor["type"] = 5;
    sensor["topic"] = "watering/status";
    const JsonArray range = docRequest["range"].to<JsonArray>();
    const JsonObject range1 = range.add<JsonObject>();
    range1["type"] = 2;
    range1["topic_cmd"] = "moisture/reference";
    range1["topic_state"] = "";
    range1["max"] = 100;
    range1["min"] = 0;
    range1["precision"] = 1;
    range1["multiplier"] = 1;
    const JsonArray mode = docRequest["mode"].to<JsonArray>();
    const JsonObject mode1 = mode.add<JsonObject>();
    mode1["type"] = 6;
    mode1["topic_cmd"] = "watering/mode";
    mode1["topic_state"] = "";
    mode1["options"] = "one=1,two=2,three=3";
    String requestBody;
    serializeJson(docRequest, requestBody);

    if (!httpClient.begin(clientSecure, ENDPOINT_DEVICE_REGISTER)) return 0;
    httpClient.addHeader("Authorization", "Token " + String(config.WQTT_TOKEN));
    httpClient.addHeader("Accept", "*/*");
    httpClient.addHeader("Content-Type", "application/json");
    if (httpClient.POST(requestBody) != 200) return 0;

    String payload = httpClient.getString();
    httpClient.end();

    JsonDocument docResponse;
    const DeserializationError error = deserializeJson(docResponse, payload);
    if (error) return 0;

    config.DEVICE_ID = docResponse["detail"]["device_id"];
    saveConfig();

    Serial.println(config.DEVICE_ID);
    return config.DEVICE_ID;
}

bool updateBroker() {
    Serial.print("Fetching broker data...");

    if (!httpClient.begin(clientSecure, ENDPOINT_DEVICE_CONNECT)) return false;
    httpClient.addHeader("Authorization", "Token " + String(config.WQTT_TOKEN));
    httpClient.addHeader("Accept", "*/*");
    if (httpClient.GET() != 200) return false;

    String payload = httpClient.getString();
    httpClient.end();

    JsonDocument docResponse;
    const DeserializationError error = deserializeJson(docResponse, payload);
    if (error) return false;

    strcpy(config.MQTT_HOST, docResponse["server"]);
    config.MQTT_HOST[sizeof(config.MQTT_HOST) - 1] = '\0';

    config.MQTT_PORT = docResponse["port"];

    strcpy(config.MQTT_USERNAME, docResponse["user"]);
    config.MQTT_USERNAME[sizeof(config.MQTT_USERNAME) - 1] = '\0';

    strcpy(config.MQTT_PASSWORD, docResponse["password"]);
    config.MQTT_PASSWORD[sizeof(config.MQTT_PASSWORD) - 1] = '\0';

    saveConfig();
    Serial.println("done.");
    return true;
}

bool willRainToday() {
    if (config.WEATHER_LATITUDE <= 0) return false;
    if (config.WEATHER_LONGITUDE <= 0) return false;

    String endpoint = ENDPOINT_WEATHER;
    endpoint.replace("%lat%", String(config.WEATHER_LATITUDE));
    endpoint.replace("%lon%", String(config.WEATHER_LONGITUDE));

    if (!httpClient.begin(clientSecure, endpoint)) return false;
    const int httpCode = httpClient.GET();
    if (httpCode != 200) return false;

    String payload = httpClient.getString();
    httpClient.end();

    JsonDocument docResponse;
    const DeserializationError error = deserializeJson(docResponse, payload);
    if (error) return false;

    const auto hourlyRain = docResponse["hourly"]["rain"].as<JsonArray>();
    for (JsonVariant mm: hourlyRain) if (mm.as<float>() > 0.0f) return true;
    return false;
}

/* Сервер */

void handleGet() {
    String content = FPSTR(HTML_PAGE_INDEX);

    content.replace("%SSID_VAL%", config.WIFI_SSID);
    content.replace("%PASS_VAL%", config.WIFI_PASSWORD);
    content.replace("%WQTT_TOKEN_VAL%", config.WQTT_TOKEN);
    content.replace("%LATITUDE_VAL%", String(config.WEATHER_LATITUDE));
    content.replace("%LONGITUDE_VAL%", String(config.WEATHER_LONGITUDE));

    webServer.send(200, "text/html", content);
}

void handlePost() {
    if (webServer.hasArg("ssid")) {
        strcpy(config.WIFI_SSID, webServer.arg("ssid").c_str());
        config.WIFI_SSID[sizeof(config.WIFI_SSID) - 1] = '\0';
    }
    if (webServer.hasArg("password")) {
        strcpy(config.WIFI_PASSWORD, webServer.arg("password").c_str());
        config.WIFI_PASSWORD[sizeof(config.WIFI_PASSWORD) - 1] = '\0';
    }
    if (webServer.hasArg("token")) {
        strcpy(config.WQTT_TOKEN, webServer.arg("token").c_str());
        config.WQTT_TOKEN[sizeof(config.WQTT_TOKEN) - 1] = '\0';
    }
    if (webServer.hasArg("latitude")) {
        config.WEATHER_LATITUDE = webServer.arg("latitude").toFloat();
    }
    if (webServer.hasArg("longitude")) {
        config.WEATHER_LONGITUDE = webServer.arg("longitude").toFloat();
    }

    saveConfig();
    handleGet();
}

void handleReset() {
    resetConfig();
    webServer.send(200, "text/plain", "Config reset. Restart in 3 seconds...");
    delay(3000);
    ESP.restart();
}

void handle404() {
    webServer.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
    webServer.send(302, "text/plain", "");
}

void setupServer() {
    webServer.on("/", HTTP_GET, handleGet);
    webServer.on("/", HTTP_POST, handlePost);
    webServer.on("/reset", HTTP_POST, handleReset);
    webServer.onNotFound(handle404);
    webServer.begin();
}

/* MQTT */

bool mqttConnect() {
    mqttClient.setServer(config.MQTT_HOST, config.MQTT_PORT);
    if (strlen(config.MQTT_HOST) == 0 && !updateBroker()) return false;
    if (config.DEVICE_ID <= 0 && registerDevice() == 0) return false;

    Serial.print("Connecting to MQTT broker...");
    for (unsigned int i = 0; i < 10; i++) {
        if (mqttClient.connect(WiFi.macAddress().c_str(), config.MQTT_USERNAME, config.MQTT_PASSWORD)) {
            Serial.println("connected");
            mqttClient.subscribe(MQTT_TOPIC_REFERENCE);
            mqttClient.subscribe(MQTT_TOPIC_MODE);
            return true;
        }
        Serial.print(".");
        delay(1000);
    }
    Serial.println("not connected");
    return false;
}

void mqttReceive(const char *topic, const byte *payload, const unsigned int length) {
    char value[length + 1];
    memcpy(value, payload, length);
    value[length] = '\0';

    Serial.print("MQTT payload received: ");
    Serial.print(topic);
    Serial.print(" - ");
    Serial.println(value);

    if (strcmp(topic, MQTT_TOPIC_REFERENCE) == 0) {
        zbSend(XBEE_COMMAND_REFERENCE, value);
    } else if (strcmp(topic, MQTT_TOPIC_MODE) == 0) {
        zbSend(XBEE_COMMAND_MODE, value);
    }
}

/* База */

void setupHost() {
    wifiShare();
    setupServer();

    Serial.println("Host is set up.");
}

void setupClient() {
    Serial2.begin(9600);

    mqttClient.setServer(config.MQTT_HOST, config.MQTT_PORT);
    mqttClient.setCallback(mqttReceive);

    xbeeClient.setSerial(Serial2);
    xbeeClient.onZBRxResponse(zbReceive);

    Serial.println("Client is set up.");
}

void setup() {
    Serial.begin(9600);
    Serial.println();

    clientSecure.setInsecure();
    loadConfig();

    pinMode(PIN_LED, OUTPUT);
    pinMode(PIN_MODE, INPUT_PULLUP);

    serverMode = digitalRead(PIN_MODE) == LOW || !wifiConnect() || !mqttConnect();
    serverMode ? setupHost() : setupClient();
}

void loopHost() {
    webServer.handleClient();
}

void loopClient() {
    mqttClient.loop();
    xbeeClient.loop();

    const unsigned long now = millis();
    if (now < weatherLast || millis() - weatherLast > WEATHER_INTERVAL) {
        zbSend(XBEE_COMMAND_RAIN, willRainToday() ? "1" : "0");
        weatherLast = millis();
    }
}

void loop() { serverMode ? loopHost() : loopClient(); }
