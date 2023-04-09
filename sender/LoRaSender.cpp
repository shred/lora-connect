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

#include "LoRaSender.h"

#include "config.h"

#define LORA_PACKAGE_RATE_LIMIT 1000
#define LORA_FLUSH_FREQUENCY 5000


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


inline static unsigned long timeDifference(unsigned long now, unsigned long past) {
  // This is actually safe from millis() overflow because all types are unsigned long!
  return past > 0 ? (now - past) : 0;
}

LoRaSender::LoRaSender() {
  sendPosition = 0;

  unsigned long now = millis();
  lastSent = now;
  lastFlush = now;
}

void LoRaSender::sendInt(uint16_t key, int32_t value) {
  Serial.printf("LR: send int %u = %d\n", key, value);

  if (value == 0) {
    sendMessage(0, key, NULL, 0);
    return;
  }

  bool negative = value < 0;
  if (negative) {
    value = -value;
  }

  if (value < 256) {
    uint8_t data[1] = {
      value & 0xFF
    };
    sendMessage(negative ? 2 : 1, key, data, sizeof(data));
    return;
  }

  if (value < 65536) {
    uint8_t data[2] = {
      value & 0xFF,
      (value >> 8) & 0xFF
    };
    sendMessage(negative ? 4 : 3, key, data, sizeof(data));
    return;
  }

  uint8_t data[4] = {
    value & 0xFF,
    (value >> 8) & 0xFF,
    (value >> 16) & 0xFF,
    (value >> 24) & 0xFF
  };
  sendMessage(negative ? 6 : 5, key, data, sizeof(data));
}

void LoRaSender::sendBoolean(uint16_t key, bool value) {
  Serial.printf("LR: send bool %u = %d\n", key, value);

  sendMessage(value ? 8 : 7, key, NULL, 0);
}

void LoRaSender::sendString(uint16_t key, String value) {
  Serial.printf("LR: send string %u = %s\n", key, value);

  sendMessage(9, key, (uint8_t *)value.c_str(), value.length() + 1);
}

void LoRaSender::sendSystemMessage(String message) {
  Serial.printf("LR: send system msg %s\n", message);

  size_t length = message.length() + 1;
  if (sendPosition + 1 + length > sizeof(sendBuffer)) {
    flush();
  }
  if (sendPosition + 1 + length > sizeof(sendBuffer)) {
    Serial.printf("LR: System Message '%s' is too big and was dropped.", message);
  }

  sendBuffer[sendPosition++] = 255;
  memcpy(sendBuffer + sendPosition, message.c_str(), length);
  sendPosition += length;

  flush();  // send system message immediately
}

void LoRaSender::sendMessage(uint8_t type, uint16_t key, uint8_t *msg, size_t length) {
  if (sendPosition + 3 + length > sizeof(sendBuffer)) {
    flush();
  }
  if (sendPosition + 3 + length > sizeof(sendBuffer)) {
    Serial.printf("LR: Message type %u, key %u, size %u is too big and was dropped.", type, key, length);
  }

  sendBuffer[sendPosition++] = type;
  sendBuffer[sendPosition++] = key & 0xFF;
  sendBuffer[sendPosition++] = (key >> 8) & 0xFF;
  if (length > 0) {
    memcpy(sendBuffer + sendPosition, msg, length);
    sendPosition += length;
  }
}

void LoRaSender::flush() {
  if (sendPosition != 0) {
    Serial.println("LR: flushing");
    sendRaw(sendBuffer, sendPosition);
    sendPosition = 0;
  }
  lastFlush = millis();
}

void LoRaSender::sleep() {
  LoRa.idle();
}

void LoRaSender::sendRaw(uint8_t *msg, size_t length) {
  // Make sure not to send more often than once per second
  unsigned long now = millis();
  unsigned long lastSentDiff = timeDifference(now, lastSent);
  if (lastSentDiff < LORA_PACKAGE_RATE_LIMIT) {
    delay(LORA_PACKAGE_RATE_LIMIT - lastSentDiff);
  }
  lastSent = now;

  Serial.println("LR: == SENDING ==");
  printBytes(msg, length);

  // TODO: Encrypt, Hash
  LoRa.beginPacket();
  LoRa.setTxPower(LORA_POWER, (LORA_PABOOST == true ? RF_PACONFIG_PASELECT_PABOOST : RF_PACONFIG_PASELECT_RFO));
  LoRa.write(msg, length);
  LoRa.endPacket();
}

void LoRaSender::loop() {
  unsigned long now = millis();

  if (timeDifference(now, lastFlush) > LORA_FLUSH_FREQUENCY) {
    flush();
  }
}
