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

#include <WiFi.h>

#include "HCSocket.h"
#include "LoRaSender.h"
#include "Utils.h"
#include "config.h"

#define LED_PIN 25

// AP connection
bool apGate = false;
bool deviceConnected = false;
uint8_t deviceAid;
IPAddress deviceIp;
uint8_t expectedMac[6] = HC_APPLIANCE_MAC;

void processMessage(const JsonDocument &message);

// Connection to appliance
HCSocket socket(HC_APPLIANCE_KEY, HC_APPLIANCE_IV, processMessage);

// LoRa
LoRaSender lora(LORA_ENCRYPT_KEY);

void WiFiApConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.printf("Connection attempt (AID %u, MAC %02X:%02X:%02X:%02X:%02X:%02X)\n",
                info.wifi_ap_staconnected.aid,
                info.wifi_ap_staconnected.mac[0],
                info.wifi_ap_staconnected.mac[1],
                info.wifi_ap_staconnected.mac[2],
                info.wifi_ap_staconnected.mac[3],
                info.wifi_ap_staconnected.mac[4],
                info.wifi_ap_staconnected.mac[5]);
  if (0 == memcmp(info.wifi_ap_staconnected.mac, expectedMac, sizeof(expectedMac))) {
    deviceConnected = false;
    deviceAid = info.wifi_ap_staconnected.aid;
    apGate = true;
    Serial.println("Appliance is connected");
    lora.sendSystemMessage("Appliance connected");
  } else {
    apGate = false;
    Serial.println("Ignored unregistered device");
    lora.sendSystemMessage("Unknown device connected");
  }
}

void WiFiApIpAssigned(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (apGate) {
    deviceIp = IPAddress(info.wifi_ap_staipassigned.ip.addr);
    deviceConnected = true;
    apGate = false;
    Serial.printf("Assigned IP %s to AID %u\n", deviceIp.toString(), deviceAid);
    socket.connect(deviceIp, 80);
    lora.sendSystemMessage("Appliance IP " + deviceIp.toString());
    digitalWrite(LED_PIN, HIGH);
  }
}

void WiFiApDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (deviceConnected && deviceAid == info.wifi_ap_stadisconnected.aid) {
    deviceConnected = false;
    apGate = false;
    lora.sendSystemMessage("Appliance disconnected");
    Serial.printf("Appliance disconnected, AID %u\n", deviceAid);
    digitalWrite(LED_PIN, LOW);
    lora.sleep();
  }
}

void processMessage(const JsonDocument &msg) {
  Serial.println("Received an event");
  serializeJson(msg, Serial);
  Serial.println();

  if ((msg["action"] == "NOTIFY" && msg["resource"] == "/ro/values")
      || (msg["action"] == "RESPONSE" && msg["resource"] == "/ro/allMandatoryValues")) {
    JsonArrayConst array = msg["data"];
    for (JsonVariantConst entry : array) {
      JsonObjectConst row = entry.as<JsonObjectConst>();
      uint16_t uid = row["uid"];

      if (row["value"].is<int32_t>()) {
        lora.sendInt(uid, row["value"]);
      } else if (row["value"].is<bool>()) {
        lora.sendBoolean(uid, row["value"]);
      } else if (row["value"].is<const char *>()) {
        lora.sendString(uid, row["value"]);
      } else {
        Serial.printf("Don't know how to send uid %u\n", uid);
      }
    }
    lora.flush();
  } else if (msg["action"] == "POST" && msg["resource"] == "/ei/initialValues") {
    socket.startSession(msg["sID"], msg["data"][0]["edMsgID"]);

    // Send reply to /ei/initialValues
    StaticJsonDocument<200> response;
    response["deviceType"] = "Application";
    response["deviceName"] = "hcpy";
    response["deviceID"] = "0badcafe";
    socket.sendReply(msg, response);

    // ask the device which services it supports
    socket.sendAction("/ci/services");

    // send authentication
    StaticJsonDocument<200> nonce;
    nonce["nonce"] = createRandomNonce();
    socket.sendActionWithData("/ci/authentication", nonce, 2);
  } else if (msg["action"] == "RESPONSE" && msg["resource"] == "/ci/authentication") {
    socket.sendAction("/ci/info", 2);
  } else if (msg["action"] == "RESPONSE" && msg["resource"] == "/ci/info") {
    socket.sendAction("/ni/info");
  } else if (msg["action"] == "RESPONSE" && msg["resource"] == "/ni/info") {
    socket.sendAction("/ei/deviceReady", 2, "NOTIFY");
    socket.sendAction("/ro/allMandatoryValues");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  randomSeed(analogRead(0));

  Serial.println("Starting Access Point");
  WiFi.disconnect(true);
  WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, AP_SSID_HIDDEN);
  WiFi.onEvent(WiFiApConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_AP_STACONNECTED);
  WiFi.onEvent(WiFiApDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_AP_STADISCONNECTED);
  WiFi.onEvent(WiFiApIpAssigned, WiFiEvent_t::ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED);

  lora.connect();

  lora.sendSystemMessage("Ready");
}

void loop() {
  socket.loop();
  lora.loop();
}
