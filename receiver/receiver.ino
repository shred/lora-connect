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
#include <heltec.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClient.h>

#include "config.h"

unsigned long beforeMqttConnection = millis();
bool connected = false;
WiFiClient wifiClient;
PubSubClient client(MQTT_SERVER_HOST, MQTT_SERVER_PORT, wifiClient);

inline static unsigned long timeDifference(unsigned long now, unsigned long past) {
  // This is actually safe from millis() overflow because all types are unsigned long!
  return past > 0 ? (now - past) : 0;
}

static void printBytes(uint8_t *data, size_t length) {
  for (int i = 0; i < length; i++) {
    Serial.printf("%02X", data[i]);
  }
  Serial.println();
  for (int i = 0; i < length; i++) {
    if (data[i] >= 32 && data[i] < 128) {
      Serial.print((char)data[i]);
    } else {
      Serial.print('.');
    }
  }
  Serial.println();
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

String mapKey(uint16_t key) {
  switch (key) {
    case 527: return F("BSH.Common.Status.DoorState");
    case 542: return F("BSH.Common.Option.ProgramProgress");
    case 544: return F("BSH.Common.Option.RemainingProgramTime");
    case 552: return F("BSH.Common.Status.OperationState");
    case 27142: return F("LaundryCare.Common.Option.ProcessPhase");
    default: return String(key, DEC);
  }
}

void onLoRaReceive(int packetSize) {
  Serial.printf("Received LoRa message, size: %d\n", packetSize);

  uint8_t packet[512];
  int len = 0;
  while (LoRa.available()) {
    packet[len++] = (uint8_t)LoRa.read();
  }
  printBytes(packet, len);

  // TODO: Decrypt
  // TODO: Check signature/checksum

  if (packet[0] != 0) {
    Serial.print("  Remote system message: ");
    Serial.println(String().concat(((char *)packet) + 1, len - 1));
  } else {
    uint16_t data[(len - 1) / 2];
    memcpy(data, packet + 1, len - 1);

    DynamicJsonDocument doc(8192);
    doc["key"] = mapKey(data[0]);
    doc["value"] = data[1];
    doc["loraSignalStrength"] = LoRa.packetRssi();
    doc["wifiSignalStrength"] = WiFi.RSSI();

    String json;
    serializeJson(doc, json);
    if (!client.publish(MQTT_TOPIC, json.c_str(), MQTT_RETAIN)) {
      Serial.print("Failed to send MQTT message, rc=");
      Serial.println(client.state());
    }
  }
}

void setup() {
  Heltec.begin(
    true,          // display is enabled
    true,          // LoRa is enabled
    true,          // serial is enabled
    LORA_PABOOST,  // set LoRa paboost
    LORA_BAND      // set LoRa band
  );
  delay(1000);

  Serial.begin(115200);
  Serial.println();
  Heltec.display->clear();
  Heltec.display->display();

  // Start WLAN
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.onEvent(onWiFiStaDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.onEvent(onWiFiStaIpAssigned, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.begin(WLAN_SSID, WLAN_PSK);
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

  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    onLoRaReceive(packetSize);
  }
}
