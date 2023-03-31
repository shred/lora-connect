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

#include "LoRaReceiver.h"

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

LoRaReceiver::LoRaReceiver() {
}

void LoRaReceiver::onReceiveInt(ReceiveIntEvent intEventListener) {
  this->intEventListener = intEventListener;
}

void LoRaReceiver::onReceiveBoolean(ReceiveBooleanEvent booleanEventListener) {
  this->booleanEventListener = booleanEventListener;
}

void LoRaReceiver::onReceiveString(ReceiveStringEvent stringEventListener) {
  this->stringEventListener = stringEventListener;
}

void LoRaReceiver::onReceiveSystemMessage(ReceiveSystemMessageEvent systemMessageEventListener) {
  this->systemMessageEventListener = systemMessageEventListener;
}

void LoRaReceiver::onLoRaReceive(size_t packetSize) {
  Serial.printf("LR: Received message, size: %u\n", packetSize);

  if (packetSize == 0) {
    Serial.println("LR: Empty message, ignoring");
    return;
  }

  receiveLength = 0;
  while (LoRa.available()) {
    if (receiveLength > sizeof(receiveBuffer)) {
      Serial.println("LR: Message exceeds buffer, ignoring");
      return;
    }
    receiveBuffer[receiveLength++] = (uint8_t)LoRa.read();
  }

  if (receiveLength != packetSize) {
    Serial.printf("LR: Expected message length was %u, but is %u\n", packetSize, receiveLength);
    return;
  }

  Serial.println("LR: == RECEIVED ==");
  printBytes(receiveBuffer, receiveLength);

  // TODO: Decrypt
  // TODO: Check signature/checksum

  cursor = 0;
  while (cursor < receiveLength) {
    uint8_t type = receiveBuffer[cursor++];
    switch (type) {
      case 0:  // uint16_t positive
      case 1: { // int16_t negative
          uint16_t key = readKey();
          int32_t value = readInteger();
          if (type == 1) {
            value = -value;
          }
          if (intEventListener) {
            intEventListener(key, value);
          }
        }
        break;

      case 2:  // boolean "false"
      case 3: { // boolean "true"
          uint16_t key = readKey();
          if (booleanEventListener) {
            booleanEventListener(key, type == 3);
          }
        }
        break;

      case 4: { // String
          uint16_t key = readKey();
          String str = readString();
          if (stringEventListener) {
            stringEventListener(key, str);
          }
        }
        break;

      case 255: { // System message
          String str = readString();
          if (systemMessageEventListener) {
            systemMessageEventListener(str);
          }
        }
        break;

      default:
        Serial.printf("Unknown message type %u, ignoring rest of message", type);
        return;
    }
  }
}

uint16_t LoRaReceiver::readKey() {
  uint16_t result = 0;
  if (cursor + 2 <= receiveLength) {
    result = (receiveBuffer[cursor] & 0xFF) | ((receiveBuffer[cursor + 1] & 0xFF) << 8);
    cursor += 2;
  }
  return result;
}

int32_t LoRaReceiver::readInteger() {
  int32_t result = 0;
  if (cursor + 2 <= receiveLength) {
    result = (receiveBuffer[cursor] & 0xFF) | ((receiveBuffer[cursor + 1] & 0xFF) << 8);
    cursor += 2;
  }
  return result;
}

String LoRaReceiver::readString() {
  size_t strlen = 0;
  uint8_t *start = receiveBuffer + cursor;
  while (receiveBuffer[cursor] != 0 && cursor < receiveLength) {
    strlen++;
    cursor++;
  }
  return String(start, strlen);
}

void LoRaReceiver::loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    onLoRaReceive(packetSize);
  }
}
