#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
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

/** 128x32, text size 2: two lines — temperature and humidity only. */
static void drawScreen(float t, float h, bool dhtOk) {
  char line[24];
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  if (dhtOk) {
    snprintf(line, sizeof(line), "T: %.1f C", t);
    display.println(line);
    snprintf(line, sizeof(line), "H: %.1f %%", h);
    display.println(line);
  } else {
    display.println(F("Sensor read"));
    display.println(F("error"));
  }

  display.display();
}

static int compareFloat(const void *a, const void *b) {
  const float fa = *static_cast<const float *>(a);
  const float fb = *static_cast<const float *>(b);
  return (fa > fb) - (fa < fb);
}

/** Median of n values (n >= 1). For even n, average of the two middle values after sorting. */
static float medianInPlaceCopy(float *values, int n) {
  static float scratch[MQTT_MAX_SAMPLES_PER_WINDOW];
  if (n <= 0) {
    return NAN;
  }
  if (n > MQTT_MAX_SAMPLES_PER_WINDOW) {
    n = MQTT_MAX_SAMPLES_PER_WINDOW;
  }
  memcpy(scratch, values, static_cast<size_t>(n) * sizeof(float));
  qsort(scratch, static_cast<size_t>(n), sizeof(float), compareFloat);
  if (n % 2 == 1) {
    return scratch[n / 2];
  }
  return (scratch[n / 2 - 1] + scratch[n / 2]) * 0.5f;
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
  static unsigned long windowStartMs = 0;
  static bool windowStarted = false;
  static float tempSamples[MQTT_MAX_SAMPLES_PER_WINDOW];
  static float moistSamples[MQTT_MAX_SAMPLES_PER_WINDOW];
  static int sampleCount = 0;

  if (WiFi.status() != WL_CONNECTED) {
    setupWiFi();
  }

  mqtt.loop();

  const unsigned long now = millis();
  if (!windowStarted) {
    windowStartMs = now;
    windowStarted = true;
  }

  if (now - lastSensorMs >= SENSOR_INTERVAL_MS) {
    lastSensorMs = now;

    const float h = dht.readHumidity();
    const float t = dht.readTemperature();
    const bool dhtOk = !isnan(h) && !isnan(t);

    if (dhtOk) {
      Serial.printf("sample T=%.2f C  H=%.2f %%\n", t, h);
      if (sampleCount < MQTT_MAX_SAMPLES_PER_WINDOW) {
        tempSamples[sampleCount] = t;
        moistSamples[sampleCount] = h;
        sampleCount++;
      }
    } else {
      Serial.println(F("DHT read failed"));
    }

    drawScreen(t, h, dhtOk);
  }

  if (now - windowStartMs >= MQTT_PUBLISH_INTERVAL_MS) {
    if (sampleCount > 0 && WiFi.status() == WL_CONNECTED && ensureMqtt()) {
      const float tempMed = medianInPlaceCopy(tempSamples, sampleCount);
      const float moistMed = medianInPlaceCopy(moistSamples, sampleCount);
      char payload[96];
      snprintf(payload, sizeof(payload),
               "{\"temp\":%.2f,\"moist\":%.2f,\"deviceId\":%d}",
               tempMed, moistMed, DEVICE_ID);
      if (!mqtt.publish(MQTT_TOPIC, payload, true)) {
        Serial.println(F("MQTT publish failed"));
      } else {
        Serial.printf("MQTT median over %d samples: T=%.2f moist=%.2f\n", sampleCount, tempMed,
                      moistMed);
      }
    } else if (sampleCount == 0) {
      Serial.println(F("No valid samples in window; skip MQTT publish"));
    }
    sampleCount = 0;
    windowStartMs = now;
  }

  delay(10);
}
