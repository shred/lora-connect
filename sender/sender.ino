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

int counter = 0;

void setup() {
    Heltec.begin(
        true,           // display is enabled
        true,           // LoRa is enabled
        true,           // serial is enabled
        LORA_PABOOST,   // set LoRa paboost
        LORA_BAND       // set LoRa band
    );
}

void loop() {
    Serial.print("Sending packet: ");
    Serial.println(counter);

    LoRa.beginPacket();
    LoRa.setTxPower(LORA_POWER, (LORA_PABOOST == true ? RF_PACONFIG_PASELECT_PABOOST : RF_PACONFIG_PASELECT_RFO));
    LoRa.print("hello ");
    LoRa.print(counter);
    LoRa.endPacket();

    counter++;
    digitalWrite(25, HIGH);     // turn the LED on
    delay(1000);                // wait for a second
    digitalWrite(25, LOW);      // turn the LED off
    delay(1000);                // wait for a second
}
