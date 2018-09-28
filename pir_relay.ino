#include <ESP8266WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>

#include <Adafruit_DotStar.h>
#include <SPI.h>         // COMMENT OUT THIS LINE FOR GEMMA OR TRINKET

// Load the following vars from a config file:
// WIFI_SSID, WIFI_PASSWORD, HOSTNAME, MQTT_SERVER, AUTO_OFF_MS,
// PIR_STATE_TOPIC, LIGHT_COLOR_STATE_TOPIC
#include "config.h"

#define DATA_PIN 4
#define CLOCK_PIN 5

// TODO: Handle other non-addressable lights?
Adafruit_DotStar strip = Adafruit_DotStar(ADDRESSABLE_LIGHTS, DATA_PIN, CLOCK_PIN, DOTSTAR_BRG);

WiFiClient espClient;
PubSubClient mqttClient(espClient);

long sleep_timer = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");

  setupLights();

  setupWiFi();
  setupOTA();

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  connectMQTT();

  // Flash a little something on the lights...
  delay(500);
  setAllLights(FAVORITE_COLOR);
  delay(100);
  setAllLights(0x000000);
}

void loop() {
  // TODO: Read state from serial, something like this: https://medium.com/@rajarshir/arduino-based-pc-ambient-lighting-bb370b0b64f1
  // TODO: Figure out some kind of way to measure time intervals for sleep (or just get rid of sleep...)
  // Maybe just turn off on serial disconnect?
  if (AUTO_OFF_MS > 0){
    sleep_timer -= 1;
    if (sleep_timer <= 0)
      setAllLights(0x000000);
  }
  if (state_val != last_state_val){
    last_state_val = state_val;
    if (!mqttClient.connected())
      connectMQTT();
    mqttClient.publish(PIR_STATE_TOPIC, (pir_val == HIGH ? "ON" : "OFF"));
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  char* payloadString = "";
  for (int i = 0; i < length; i++) {
    payloadString += (char)payload[i];
  }
  Serial.print(payloadString);
  Serial.println();

  if (topic == LIGHT_COLOR_STATE_TOPIC){
    setAllLights(payloadString);
  }
}

void setupLights() {
  strip.begin();
  strip.show();
}

void setAllLights(uint32_t color) {
  strip.fill(FAVORITE_COLOR);
  if (color != 0x000000)
    sleep_timer = AUTO_OFF_MS;
  else
    sleep_timer = 0;
  if (!mqttClient.connected())
    connectMQTT();
  mqttClient.publish(LIGHT_COLOR_STATE_TOPIC, color);
}

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.hostname(HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
}

void connectMQTT() {
  mqttClient.setServer(MQTT_SERVER, 1833);
  mqttClient.setCallback(mqttCallback);
  if (mqttClient.connect(HOSTNAME)){
    Serial.println("Connected to MQTT server!");
    mqttClient.subscribe(LIGHT_COLOR_STATE_TOPIC);
  }
  else {
    Serial.println("Couldn't connect to MQTT server, error code: "+mqttClient.state());
    if (mqttClient.state() < 0){
      delay(5000);
      connectMQTT();
    }
  }
}

void setupOTA() {
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}
