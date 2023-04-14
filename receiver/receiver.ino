/*
 * LoRa-Connect
 *
 * Copyright (C) 2023 Richard "Shred" Körber
 *   https://github.com/shred/lora-connect
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <cppQueue.h>

#include "LoRaReceiver.h"

#include "config.h"
#include "mapping.h"

#define LED_PIN 25

#define JSONQUEUE_SIZE 64
#define JSONQUEUE_MESSAGE_SIZE 512

unsigned long beforeMqttConnection = millis();
bool connected = false;
WiFiClient wifiClient;
LoRaReceiver lora(LORA_ENCRYPT_KEY);
PubSubClient client(MQTT_SERVER_HOST, MQTT_SERVER_PORT, wifiClient);
cppQueue jsonQueue(JSONQUEUE_MESSAGE_SIZE, JSONQUEUE_SIZE, FIFO, true);


void onReceiveInt(uint16_t key, int32_t value) {
  Serial.printf("HC: RECEIVED int %u = %d\n", key, value);

  DynamicJsonDocument doc(1024);
  String keyStr = mapKey(key);
  doc["uid"] = key;
  doc["key"] = keyStr;
  doc["value"] = value;

  String mapped = mapIntValue(key, value);
  if (mapped.length() > 0) {
    doc["exp"] = mapped;
  } else if (keyStr == F("BSH.Common.Root.SelectedProgram")
             || keyStr == F("BSH.Common.Root.ActiveProgram")
             || keyStr == F("LaundryCare.Common.Option.ReferToProgram")) {
    doc["exp"] = mapKey(value);
  } else if (keyStr == F("BSH.Common.Option.RemainingProgramTime")
             || keyStr == F("BSH.Common.Option.EstimatedTotalProgramTime")
             || keyStr == F("BSH.Common.Option.FinishInRelative")) {
    int remainHr = value / 3600;
    int remainMin = (value / 60) % 60;
    char buff[30];
    snprintf(buff, sizeof(buff), "%1u:%02u", remainHr, remainMin);
    doc["exp"] = String(buff);
  } else if (keyStr == F("BSH.Common.Option.ProgramProgress")) {
    char buff[30];
    snprintf(buff, sizeof(buff), "%d%%", value);
    doc["exp"] = String(buff);
  }

  postToMqtt(doc);
}

void onReceiveBoolean(uint16_t key, bool value) {
  Serial.printf("HC: RECEIVED bool %u = %d\n", key, value);

  DynamicJsonDocument doc(1024);
  doc["uid"] = key;
  doc["key"] = mapKey(key);
  doc["value"] = value;
  postToMqtt(doc);
}

void onReceiveString(uint16_t key, String value) {
  Serial.printf("HC: RECEIVED str %u = '%s'\n", key, value.c_str());

  DynamicJsonDocument doc(1024);
  doc["uid"] = key;
  doc["key"] = mapKey(key);
  doc["value"] = value;
  postToMqtt(doc);
}

void onReceiveSystemMessage(String value) {
  Serial.printf("HC: RECEIVED sender message '%s'\n", value.c_str());

  DynamicJsonDocument doc(1024);
  doc["systemMessage"] = value;
  postToMqtt(doc);
}

void onWiFiStaIpAssigned(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.printf("MQ: Connected to WLAN with IP %s\n", IPAddress(info.got_ip.ip_info.ip.addr).toString());
  connected = true;
}

void onWiFiStaDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  connected = false;
  Serial.println("MQ: Reconnecting to WLAN");
  WiFi.begin(WLAN_SSID, WLAN_PSK);
}

void postToMqtt(DynamicJsonDocument &doc) {
  doc["loraSignalStrength"] = lora.getRssi();
  doc["wifiSignalStrength"] = WiFi.RSSI();

  char json[JSONQUEUE_MESSAGE_SIZE];
  if (serializeJson(doc, json, JSONQUEUE_MESSAGE_SIZE) >= JSONQUEUE_MESSAGE_SIZE - 1) {
    Serial.println("MQ: JSON message exceeded buffer and was dropped!");
    return;
  }
  if (!jsonQueue.push(json)) {
    Serial.println("MQ: JSON queue is full, message was dropped!");
  }
}

bool sendMqttMessage(char *message) {
  Serial.printf("MQ: Sending %s\n", message);
  if (!client.publish(MQTT_TOPIC, message, MQTT_RETAIN)) {
    Serial.printf("MQ: Sending failed, rc=%d\n", client.state());
    return false;
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  Serial.println();

  // Turn LED off
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Start WLAN
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.onEvent(onWiFiStaDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.onEvent(onWiFiStaIpAssigned, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.begin(WLAN_SSID, WLAN_PSK);

  // Start LoRa
  lora.onReceiveInt(onReceiveInt);
  lora.onReceiveBoolean(onReceiveBoolean);
  lora.onReceiveString(onReceiveString);
  lora.onReceiveSystemMessage(onReceiveSystemMessage);
  lora.connect();
}

void loop() {
  const unsigned long now = millis();

  if (connected) {
    if (!client.loop() && (now - beforeMqttConnection) > MQTT_RECONNECT_DELAY) {
      beforeMqttConnection = now;
      if (client.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
        Serial.println("MQ: Connected to MQTT server");
      } else {
        Serial.printf("MQ: Failed to connect to MQTT server, rc=%d\n", client.state());
      }
    }
  }

  if (client.connected()) {
    char message[JSONQUEUE_MESSAGE_SIZE];
    if (jsonQueue.peek(&message)) {
      if (sendMqttMessage(message)) {
        jsonQueue.drop();
      }
    }
  }

  lora.loop();
}
