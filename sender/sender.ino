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

#include "config.h"

// AP connection
boolean apGate = false;
boolean deviceConnected = false;
uint8_t deviceAid;
esp_ip4_addr_t deviceIp;
uint8_t expectedMac[6] = HC_DEVICE_MAC;

void WiFiApConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.printf("Connection attempt (AID %d, MAC %02X:%02X:%02X:%02X:%02X:%02X)\n",
    info.wifi_ap_staconnected.aid,
    info.wifi_ap_staconnected.mac[0],
    info.wifi_ap_staconnected.mac[1],
    info.wifi_ap_staconnected.mac[2],
    info.wifi_ap_staconnected.mac[3],
    info.wifi_ap_staconnected.mac[4],
    info.wifi_ap_staconnected.mac[5]
  );
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

void WiFiApIpAssigned(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (apGate) {
    deviceIp = info.wifi_ap_staipassigned.ip;
    deviceConnected = true;
    apGate = false;
    Serial.printf("Assigned IP " IPSTR " to AID %d\n", IP2STR(&deviceIp), deviceAid);
  }
}

void WiFiApDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (deviceConnected && deviceAid == info.wifi_ap_stadisconnected.aid) {
    deviceConnected = false;
    apGate = false;
    Serial.printf("Appliance disconnected, AID %d\n", deviceAid);
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

  LoRa.setTxPower(LORA_POWER, (LORA_PABOOST == true ? RF_PACONFIG_PASELECT_PABOOST : RF_PACONFIG_PASELECT_RFO));

  Serial.println("Starting Access Point");
  WiFi.disconnect(true);
  WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, AP_SSID_HIDDEN);
  WiFi.onEvent(WiFiApConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_AP_STACONNECTED);
  WiFi.onEvent(WiFiApDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_AP_STADISCONNECTED);
  WiFi.onEvent(WiFiApIpAssigned, WiFiEvent_t::ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED);
}

void loop() {
  delay(2000);
}
