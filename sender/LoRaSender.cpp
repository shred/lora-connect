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

#include <Arduino.h>
#include <cppQueue.h>
#include <heltec.h>

#include "base64url.h"
#include "LoRaSender.h"

#include "config.h"

#define LORA_PACKAGE_RATE_LIMIT 1000
#define LORA_ACK_TIMEOUT 1000


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

LoRaSender::LoRaSender(const char *base64key) {
  unsigned long now = millis();
  lastSent = now;

  uint8_t key[32];
  if (!base64UrlDecode(base64key, key, sizeof(key))) {
    Serial.println("LR: FATAL: key is invalid, check your config.h!");
  }

  hmacSha256.resetHMAC(key, sizeof(key));
  hmacSha256.update("LORAENC", 7);
  hmacSha256.finalizeHMAC(key, sizeof(key), enckey, sizeof(enckey));

  hmacSha256.resetHMAC(key, sizeof(key));
  hmacSha256.update("LORAMAC", 7);
  hmacSha256.finalizeHMAC(key, sizeof(key), mackey, sizeof(mackey));

  aesEncrypt.clear();
  if (!aesEncrypt.setKey(enckey, aesEncrypt.keySize())) {
    throw std::invalid_argument("Invalid key");
  }
  aesDecrypt.clear();
  if (!aesDecrypt.setKey(enckey, aesEncrypt.keySize())) {
    throw std::invalid_argument("Invalid key");
  }

  senderQueue = new cppQueue(sizeof(Payload), PAYLOAD_BUFFER_SIZE);
}

LoRaSender::~LoRaSender() {
  delete senderQueue;
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
  if (payloadBuffer.length + 1 + length > sizeof(payloadBuffer.data)) {
    flush();
  }
  if (payloadBuffer.length + 1 + length > sizeof(payloadBuffer.data)) {
    Serial.printf("LR: System Message '%s' is too big and was dropped.", message);
  }

  payloadBuffer.data[payloadBuffer.length++] = 255;
  memcpy(payloadBuffer.data + payloadBuffer.length, message.c_str(), length);
  payloadBuffer.length += length;

  flush();  // send system message immediately
}

void LoRaSender::sendMessage(uint8_t type, uint16_t key, uint8_t *msg, size_t length) {
  if (payloadBuffer.length + 3 + length > sizeof(payloadBuffer.data)) {
    flush();
  }
  if (payloadBuffer.length + 3 + length > sizeof(payloadBuffer.data)) {
    Serial.printf("LR: Message type %u, key %u, size %u is too big and was dropped.", type, key, length);
  }

  payloadBuffer.data[payloadBuffer.length++] = type;
  payloadBuffer.data[payloadBuffer.length++] = key & 0xFF;
  payloadBuffer.data[payloadBuffer.length++] = (key >> 8) & 0xFF;
  if (length > 0) {
    memcpy(payloadBuffer.data + payloadBuffer.length, msg, length);
    payloadBuffer.length += length;
  }
}

void LoRaSender::flush() {
  if (payloadBuffer.length != 0) {
    Serial.println("LR: flushing");
    sendRaw(payloadBuffer);
    payloadBuffer.number++;
    payloadBuffer.length = 0;
  }
}

void LoRaSender::sleep() {
  LoRa.idle();
}

void LoRaSender::sendRaw(Payload &payload) {
  if (!senderQueue->push(&payload)) {
    Serial.println("LR: Queue is full, payload dropped!");
  }
}

void LoRaSender::loop() {
  // Make sure to stay within the permitted duty cycle!
  unsigned long lastSentDiff = timeDifference(millis(), lastSent);
  if (lastSentDiff < LORA_PACKAGE_RATE_LIMIT) {
    return;
  }

  // Fetch payload from queue
  Payload sendPayload;
  if (!senderQueue->pop(&sendPayload)) {
    // Queue is empty
    return;
  }

  if (sendPayload.length == 0) {
    // Payload is empty
    return;
  }

  // Give package a random message number
  sendPayload.number = random(65536);

  // Clear hash for correct hashing
  memset(sendPayload.hash, 0, sizeof(sendPayload.hash));

  // Fill unused payload part with random numbers
  for (int ix = sendPayload.length; ix < sizeof(sendPayload.data); ix++) {
    sendPayload.data[ix] = random(256);
  }

  // Compute hash
  // We will only use the first bytes of that hash, for space reasons.
  // It is still better than nothing.
  uint8_t hash[sizeof(sendPayload.hash)];
  hmacSha256.resetHMAC(mackey, sizeof(mackey));
  hmacSha256.update(&sendPayload, sizeof(sendPayload));
  hmacSha256.finalizeHMAC(mackey, sizeof(mackey), hash, sizeof(hash));
  memcpy(sendPayload.hash, hash, sizeof(sendPayload.hash));

  Serial.println("LR: == SENDING ==");
  printBytes((uint8_t *)&sendPayload, sizeof(sendPayload));

  // Encrypt
  // TODO: This is rather weak, use a better mode of operation here!
  const uint8_t *clearBuffer = (const uint8_t *)&sendPayload;
  uint8_t cryptBuffer[sizeof(sendPayload)];
  size_t blockSize = aesEncrypt.blockSize();
  for (int ix = 0; ix < sizeof(cryptBuffer); ix += blockSize) {
    aesEncrypt.encryptBlock(cryptBuffer + ix, clearBuffer + ix);
  }

  Serial.println("LR: Encrypted");
  printBytes(cryptBuffer, sizeof(cryptBuffer));

  yield();

  for (int attempt = 1; attempt < 4; attempt++) {
    Serial.printf("LR: Sending, attempt %u\n", attempt);
    yield();
    LoRa.beginPacket();
    LoRa.setTxPower(LORA_POWER, (LORA_PABOOST == true ? RF_PACONFIG_PASELECT_PABOOST : RF_PACONFIG_PASELECT_RFO));
    LoRa.write(cryptBuffer, sizeof(cryptBuffer));
    LoRa.endPacket();
    LoRa.flush();
    yield();
    delay(50);

    lastSent = millis();

    while (timeDifference(millis(), lastSent) < LORA_ACK_TIMEOUT) {
      // Wait for acknowledge
      int packetSize = LoRa.parsePacket();
      yield();
      if (packetSize == 0) {
        continue;
      }
      Serial.printf("LR: received possible package %u\n", packetSize);
      if (packetSize == sizeof(Acknowledge)) {
        bool isAck = checkAcknowledge(sendPayload.number);
        if (isAck) {
          return;  // TODO: This is ugly!
        }
      }
    }
  }
  Serial.println("LR: Not acknowledged, maybe receiver is offline?");
}

bool LoRaSender::checkAcknowledge(uint16_t expectedNumber) {
  uint8_t cryptBuffer[sizeof(Acknowledge)];

  size_t receiveLength = 0;
  while (LoRa.available()) {
    if (receiveLength > sizeof(cryptBuffer)) {
      Serial.println("LR: Acknowledge package exceeds buffer, ignoring");
      return false;
    }
    cryptBuffer[receiveLength++] = (uint8_t)LoRa.read();
  }

  if (receiveLength != sizeof(Acknowledge)) {
    Serial.printf("LR: Expected acknowledge length was %u, but is %u\n", sizeof(Acknowledge), receiveLength);
    return false;
  }

  Serial.println("LR: ACK Encrypted");
  printBytes(cryptBuffer, sizeof(cryptBuffer));

  Acknowledge unencrypted;
  aesDecrypt.decryptBlock((uint8_t *)&unencrypted, cryptBuffer);

  Serial.println("LR: ACK Decrypted");
  printBytes((uint8_t *)&unencrypted, sizeof(unencrypted));

  uint8_t theirHash[sizeof(unencrypted.hash)];
  memcpy(theirHash, unencrypted.hash, sizeof(theirHash));
  memset(unencrypted.hash, 0, sizeof(unencrypted.hash));

  uint8_t ourHash[sizeof(unencrypted.hash)];
  hmacSha256.resetHMAC(mackey, sizeof(mackey));
  hmacSha256.update(&unencrypted, sizeof(unencrypted));
  hmacSha256.finalizeHMAC(mackey, sizeof(mackey), ourHash, sizeof(ourHash));

  if (0 != memcmp(theirHash, ourHash, sizeof(ourHash))) {
    Serial.println("LR: Bad acknowledge HMAC");
    return false;
  }

  if (unencrypted.number != expectedNumber) {
    Serial.println("LR: Bad acknowledge package number");
    return false;
  }

  return true;
}
