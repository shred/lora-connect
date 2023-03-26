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

#include <heltec.h>
#include <WiFi.h>

#include "HCSocket.h"
#include "config.h"

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

// AP connection
boolean apGate = false;
boolean deviceConnected = false;
uint8_t deviceAid;
IPAddress deviceIp;
uint8_t expectedMac[6] = HC_APPLIANCE_MAC;

void processMessage(const JsonDocument &message);

// Connection to appliance
HCSocket socket = HCSocket(HC_APPLIANCE_KEY, HC_APPLIANCE_IV, processMessage);

void WiFiApConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.printf("Connection attempt (AID %d, MAC %02X:%02X:%02X:%02X:%02X:%02X)\n",
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
  } else {
    apGate = false;
    Serial.println("Ignored unregistered device");
  }
}

void sendLoRaPacket(uint8_t type, char *msg) {
  sendLoRaPacket(type, msg, strlen(msg));
}

void sendLoRaPacket(uint8_t type, void *msg, size_t length) {
  Serial.printf("LORA: Sending package type %u, length %u\n", type, length);
  printBytes((uint8_t *)msg, length);

  // TODO: encrypt, hash, secure
  LoRa.beginPacket();
  LoRa.setTxPower(LORA_POWER, (LORA_PABOOST == true ? RF_PACONFIG_PASELECT_PABOOST : RF_PACONFIG_PASELECT_RFO));
  LoRa.write(type);
  LoRa.write((uint8_t *)msg, length);
  LoRa.endPacket();
}

void processMessage(const JsonDocument &msg) {
  Serial.println("Received an event");
  serializeJson(msg, Serial);
  Serial.println();

  if (msg["action"] == "NOTIFY" && msg["resource"] == "/ro/values") {
    JsonArrayConst array = msg["data"];
    size_t outputSize = array.size();
    if (outputSize > 8) {
      Serial.println("Too many data, need to split it (TODO)");
      outputSize = 8;
    }

    Serial.printf("Output size: %u\n", outputSize);

    int16_t outputBuffer[outputSize * 2];
    for (int ix = 0; ix < outputSize; ix++) {
      JsonObjectConst row = array[ix].as<JsonObjectConst>();
      outputBuffer[ix * 2] = (int16_t)row["uid"];
      outputBuffer[ix * 2 + 1] = (int16_t)row["value"];
    }

    sendLoRaPacket(0, outputBuffer, sizeof(outputBuffer));
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
    nonce["nonce"] = socket.createRandomNonce();
    socket.sendActionWithData("/ci/authentication", nonce, 2);
  } else if (msg["action"] == "RESPONSE" && msg["resource"] == "/ci/authentication") {
    socket.sendAction("/ci/info", 2);
  } else if (msg["action"] == "RESPONSE" && msg["resource"] == "/ci/info") {
    socket.sendAction("/ni/info");
  } else if (msg["action"] == "RESPONSE" && msg["resource"] == "/ni/info") {
    socket.sendAction("/ei/deviceReady", 2, "NOTIFY");
    // socket.sendAction("/ro/allDescriptionChanges");
    // socket.sendAction("/ro/allMandatoryValues");
  }
}

void WiFiApIpAssigned(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (apGate) {
    deviceIp = IPAddress(info.wifi_ap_staipassigned.ip.addr);
    deviceConnected = true;
    apGate = false;
    Serial.printf("Assigned IP %s to AID %d\n", deviceIp.toString(), deviceAid);
    socket.connect(deviceIp, 80);
    sendLoRaPacket(254, "Washer connected");
  }
}

void WiFiApDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (deviceConnected && deviceAid == info.wifi_ap_stadisconnected.aid) {
    deviceConnected = false;
    apGate = false;
    Serial.printf("Appliance disconnected, AID %d\n", deviceAid);
    sendLoRaPacket(255, "Washer disconnected");
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

  Serial.begin(115200);
  Serial.println();
  Heltec.display->clear();
  Heltec.display->display();

  randomSeed(analogRead(0));

  Serial.println("Starting Access Point");
  WiFi.disconnect(true);
  WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, AP_SSID_HIDDEN);
  WiFi.onEvent(WiFiApConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_AP_STACONNECTED);
  WiFi.onEvent(WiFiApDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_AP_STADISCONNECTED);
  WiFi.onEvent(WiFiApIpAssigned, WiFiEvent_t::ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED);

  sendLoRaPacket(253, "Reboot");
}

void loop() {
  socket.loop();
}
