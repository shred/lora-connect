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

#include "config.h"

String rssi = "RSSI --";
String packSize = "--";
String packet ;

void LoRaData(){
    Heltec.display->clear();
    Heltec.display->setTextAlignment(TEXT_ALIGN_LEFT);
    Heltec.display->setFont(ArialMT_Plain_10);
    Heltec.display->drawString(0 , 15 , "Received "+ packSize + " bytes");
    Heltec.display->drawStringMaxWidth(0 , 26 , 128, packet);
    Heltec.display->drawString(0, 0, rssi);
    Heltec.display->display();
}

void cbk(int packetSize) {
    packet = "";
    packSize = String(packetSize,DEC);
    for (int i = 0; i < packetSize; i++) {
        packet += (char) LoRa.read();
    }
    rssi = "RSSI " + String(LoRa.packetRssi(), DEC) ;
    LoRaData();
}

void setup() {
    Heltec.begin(
        true,           // display is enabled
        true,           // LoRa is enabled
        true,           // serial is enabled
        LORA_PABOOST,   // set LoRa paboost
        LORA_BAND       // set LoRa band
    );

    Heltec.display->init();
    Heltec.display->clear();
    Heltec.display->drawString(0, 0, "Heltec.LoRa Initial success!");
    Heltec.display->drawString(0, 10, "Wait for incoming data...");
    Heltec.display->display();

    delay(1000);
    LoRa.setTxPower(LORA_POWER, (LORA_PABOOST == true ? RF_PACONFIG_PASELECT_PABOOST : RF_PACONFIG_PASELECT_RFO));
    LoRa.receive();
}

void loop() {
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
        cbk(packetSize);
    }
    delay(10);
}
