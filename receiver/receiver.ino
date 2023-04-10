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

#include "LoRaReceiver.h"

#include "config.h"
#include "mapping.h"

unsigned long beforeMqttConnection = millis();
bool connected = false;
WiFiClient wifiClient;
LoRaReceiver lora(LORA_ENCRYPT_KEY);
PubSubClient client(MQTT_SERVER_HOST, MQTT_SERVER_PORT, wifiClient);

inline static unsigned long timeDifference(unsigned long now, unsigned long past) {
  // This is actually safe from millis() overflow because all types are unsigned long!
  return past > 0 ? (now - past) : 0;
}

void onReceiveInt(uint16_t key, int32_t value) {
  Serial.printf("LR: RECEIVED int %u = %d\n", key, value);

  DynamicJsonDocument doc(1024);
  String keyStr = mapKey(key);
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

  doc["uid"] = key;
  postToMqtt(doc);
}

void onReceiveBoolean(uint16_t key, bool value) {
  Serial.printf("LR: RECEIVED bool %u = %d\n", key, value);

  DynamicJsonDocument doc(1024);
  doc["key"] = mapKey(key);
  doc["value"] = value;
  doc["uid"] = key;
  postToMqtt(doc);
}

void onReceiveString(uint16_t key, String value) {
  Serial.printf("LR: RECEIVED str %u = '%s'\n", key, value.c_str());

  DynamicJsonDocument doc(1024);
  doc["key"] = mapKey(key);
  doc["value"] = value;
  doc["uid"] = key;
  postToMqtt(doc);
}

void onReceiveSystemMessage(String value) {
  Serial.printf("LR: RECEIVED system message '%s'\n", value.c_str());

  DynamicJsonDocument doc(1024);
  doc["systemMessage"] = value;
  postToMqtt(doc);
}

void onWiFiStaIpAssigned(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.printf("Connected to WLAN with IP " IPSTR "\n", IP2STR(&info.got_ip.ip_info.ip));
  connected = true;
}

void onWiFiStaDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  connected = false;
  Serial.println("WiFi connection lost. Reconnecting...");
  WiFi.begin(WLAN_SSID, WLAN_PSK);
}

void postToMqtt(DynamicJsonDocument &doc) {
  doc["loraSignalStrength"] = lora.getRssi();
  doc["wifiSignalStrength"] = WiFi.RSSI();

  String json;
  serializeJson(doc, json);
  Serial.printf("MQ: Sending %s\n", json.c_str());
  if (!client.publish(MQTT_TOPIC, json.c_str(), MQTT_RETAIN)) {
    Serial.print("Failed to send MQTT message, rc=");
    Serial.println(client.state());
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();

  // Start WLAN
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.onEvent(onWiFiStaDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.onEvent(onWiFiStaIpAssigned, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.begin(WLAN_SSID, WLAN_PSK);

  lora.onReceiveInt(onReceiveInt);
  lora.onReceiveBoolean(onReceiveBoolean);
  lora.onReceiveString(onReceiveString);
  lora.onReceiveSystemMessage(onReceiveSystemMessage);
  lora.connect();
}

void loop() {
  const unsigned long now = millis();

  if (connected) {
    if (!client.loop() && timeDifference(now, beforeMqttConnection) > 1000) {
      beforeMqttConnection = now;
      if (client.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
        Serial.println("Successfully connected to MQTT server");
      } else {
        Serial.printf("Connection to MQTT server failed, rc=%d\n", client.state());
      }
    }
  }

  lora.loop();
}
