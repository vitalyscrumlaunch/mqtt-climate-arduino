#include <Arduino.h>
#include <cstdio>
#include <WiFi.h>
#include <Wire.h>
#include <DHT.h>
#include <PubSubClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "config.h"

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

DHT dht(DHT_PIN, DHT21);

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

static void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print(F("WiFi "));
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000UL) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("IP "));
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(F("WiFi connect failed"));
  }
}

static bool ensureMqtt() {
  if (!mqtt.connected()) {
    Serial.print(F("MQTT "));
    bool ok;
    if (strlen(MQTT_USER) > 0) {
      ok = mqtt.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD);
    } else {
      ok = mqtt.connect(MQTT_CLIENT_ID);
    }
    Serial.println(ok ? F("connected") : F("failed"));
    if (!ok) {
      Serial.println(mqtt.state());
    }
    return ok;
  }
  return true;
}

static void drawScreen(float t, float h, bool dhtOk, bool mqttOk) {
  char line[24];
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  // display.println(F("DHT21 (1-wire data)"));

  if (dhtOk) {
    snprintf(line, sizeof(line), "T: %.1f C", t);
    display.println(line);
    snprintf(line, sizeof(line), "H: %.1f %%", h);
    display.println(line);
  } else {
    display.println(F("Sensor read error"));
  }

  display.println();
  snprintf(line, sizeof(line), "WiFi: %s",
           WiFi.status() == WL_CONNECTED ? "OK" : "--");
  display.println(line);
  snprintf(line, sizeof(line), "MQTT: %s", mqttOk ? "OK" : "--");
  display.println(line);
  display.display();
}

void setup() {
  Serial.begin(115200);
  delay(200);

  Wire.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
    Serial.println(F("SSD1306 init failed; check I2C wiring/address"));
  } else {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(F("Starting..."));
    display.display();
  }

  dht.begin();

  setupWiFi();
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
}

void loop() {
  static unsigned long lastSensorMs = 0;
  static unsigned long lastMQTTMs = 0;

  if (WiFi.status() != WL_CONNECTED) {
    setupWiFi();
  }

  mqtt.loop();

  const unsigned long now = millis();
  if (now - lastSensorMs < SENSOR_INTERVAL_MS) {
    delay(10);
    return;
  }
  lastSensorMs = now;

  const float h = dht.readHumidity();
  const float t = dht.readTemperature();
  const bool dhtOk = !isnan(h) && !isnan(t);

  if (dhtOk) {
    Serial.printf("T=%.2f C  H=%.2f %%\n", t, h);
  } else {
    Serial.println(F("DHT read failed"));
  }

  const unsigned long mqttNow = millis();
  if (mqttNow - lastMQTTMs < MQTT_INTERVAL_MS) {
    delay(10);
    return;
  }
  lastMQTTMs = mqttNow;
  bool mqttOk = false;
  if (WiFi.status() == WL_CONNECTED && dhtOk && ensureMqtt()) {
    char payload[96];
    snprintf(payload, sizeof(payload), "{\"temp\":%.2f,\"hum\":%.2f,\"id\":%d,\"ok\":true}",
             t, h, DEVICE_ID);
    mqttOk = mqtt.publish(MQTT_TOPIC, payload, true);
    if (!mqttOk) {
      Serial.println(F("MQTT publish failed"));
    }
  }
}