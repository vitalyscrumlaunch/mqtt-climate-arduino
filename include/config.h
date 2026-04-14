#pragma once

// WiFi
static const char *WIFI_SSID = "Keenetic-0161";
static const char *WIFI_PASSWORD = "FtvuXzZH";

// MQTT broker (e.g. Mosquitto on LAN or cloud host)
static const char *MQTT_HOST = "192.168.1.125";
static const uint16_t MQTT_PORT = 1883;
static const char *MQTT_USER = "";  // optional; leave empty if not used
static const char *MQTT_PASSWORD = "";
static const char *MQTT_TOPIC = "esp32/dht21/data";
static const char *MQTT_CLIENT_ID = "esp32-dht21";
static const int DEVICE_ID = 2;

// DHT21 (AM2301): one data pin — proprietary single-wire protocol (not Dallas 1-Wire)
static const int DHT_PIN = 4;

// I2C OLED (SSD1306), typical ESP32 defaults: SDA=GPIO21, SCL=GPIO22
static const int OLED_WIDTH = 128;
static const int OLED_HEIGHT = 32;
static const uint8_t OLED_I2C_ADDR = 0x3C; // 0x3C;  // try 0x3D if the display stays black

// Sensor read interval (ms); DHT21 needs >= ~2 s between reads
static const unsigned long SENSOR_INTERVAL_MS = 2500;
static const unsigned long MQTT_INTERVAL_MS = 60000;