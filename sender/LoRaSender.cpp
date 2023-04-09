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


LoRaSender::LoRaSender(const char *base64key) {
  lastSendTime = millis();
  nextSendDelay = 0;

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

  validEncrypted = false;
  senderQueue = new cppQueue(sizeof(Payload), PAYLOAD_BUFFER_SIZE);
  acknowledgeQueue = new cppQueue(sizeof(Acknowledge), PAYLOAD_BUFFER_SIZE);
}

LoRaSender::~LoRaSender() {
  delete acknowledgeQueue;
  delete senderQueue;
}

void LoRaSender::connect() {
  LoRa.setTxPower(LORA_POWER, (LORA_PABOOST == true ? RF_PACONFIG_PASELECT_PABOOST : RF_PACONFIG_PASELECT_RFO));
  yield();
  LoRa.receive();
  yield();
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
  Serial.printf("LR: send string %u = '%s'\n", key, value.c_str());

  sendMessage(9, key, (uint8_t *)value.c_str(), value.length() + 1);
}

void LoRaSender::sendSystemMessage(String message) {
  Serial.printf("LR: send system msg '%s'\n", message.c_str());

  size_t length = message.length() + 1;
  if (payloadBuffer.length + 1 + length > sizeof(payloadBuffer.data)) {
    flush();
  }
  if (payloadBuffer.length + 1 + length > sizeof(payloadBuffer.data)) {
    Serial.printf("LR: System Message '%s' is too big and was dropped.\n", message);
    return;
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
    Serial.printf("LR: Message type %u, key %u, size %u is too big and was dropped.\n", type, key, length);
    return;
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

void LoRaSender::onLoRaReceive(int packetSize) {
  if (packetSize != sizeof(Acknowledge)) {
    Serial.printf("LRC: Unexpected message length %u, ignoring\n", packetSize);
    return;
  }

  uint8_t cryptBuffer[sizeof(Acknowledge)];

  size_t receiveLength = 0;
  while (LoRa.available() && receiveLength < sizeof(cryptBuffer)) {
    cryptBuffer[receiveLength++] = (uint8_t)LoRa.read();
  }
  if (receiveLength != packetSize) {
    Serial.printf("LRC: Expected message length was %u, but is %u\n", packetSize, receiveLength);
    return;
  }

  if (acknowledgeQueue->push(&cryptBuffer)) {
    Serial.printf("LRC: Received acknowledge message, length %u\n", packetSize);
  } else {
    Serial.println("LRC: Queue is full, payload dropped!");
  }
}

void LoRaSender::loop() {
  if (validEncrypted) {
    // Check if we got an acknowledge already?
    uint8_t ackPackage[sizeof(Acknowledge)];
    if (acknowledgeQueue->pop(ackPackage)) {
      if (checkAcknowledge(ackPackage)) {
        validEncrypted = false;
      }
    }
  }
  yield();

  if (validEncrypted) {
    if ((millis() - lastSendTime) > nextSendDelay) {
      attempts++;
      if (attempts <= 3) {
        Serial.printf("LR: Sending package, attempt %u\n", attempts);
        transmitPayload();
        lastSendTime = millis();
        nextSendDelay = LORA_PACKAGE_RATE_LIMIT + random(100);
      } else {
        Serial.println("LR: Maximum number of reattempts reached, maybe the receiver is offline.");
        validEncrypted = false;
      }
    }
  }
  yield();

  if (!validEncrypted) {
    // No message there, take and encrypt one
    Payload sendPayload;
    if (senderQueue->pop(&sendPayload)) {
      encryptPayload(sendPayload);
      attempts = 0;
      validEncrypted = true;
    }
  }
  yield();
}

void LoRaSender::encryptPayload(Payload &sendPayload) {
  // Reduce package to minimum required length
  size_t grossPayloadLength = sendPayload.length
                              + sizeof(sendPayload.hash)
                              + sizeof(sendPayload.number)
                              + sizeof(sendPayload.length);
  currentEncryptedLength = (grossPayloadLength + 15) / 16 * 16;

  // Give package a random message number
  sendPayload.number = random(65536);
  currentPayloadNumber = sendPayload.number;

  // Fill unused payload part with random numbers
  for (int ix = sendPayload.length; ix < sizeof(sendPayload.data); ix++) {
    sendPayload.data[ix] = random(256);
  }

  // Compute hash
  // We will only use the first bytes of that hash, for space reasons.
  // It is still better than nothing.
  hmacSha256.resetHMAC(mackey, sizeof(mackey));
  hmacSha256.update(((uint8_t *)&sendPayload) + sizeof(sendPayload.hash), currentEncryptedLength - sizeof(sendPayload.hash));
  hmacSha256.finalizeHMAC(mackey, sizeof(mackey), sendPayload.hash, sizeof(sendPayload.hash));

  Serial.println("LR: == SENDING ==");
  printBytes((uint8_t *)&sendPayload, currentEncryptedLength);

  // Encrypt
  // TODO: This is rather weak, use a better mode of operation here!
  const uint8_t *clearBuffer = (const uint8_t *)&sendPayload;
  size_t blockSize = aesEncrypt.blockSize();
  for (int ix = 0; ix < currentEncryptedLength; ix += blockSize) {
    aesEncrypt.encryptBlock(currentEncrypted + ix, clearBuffer + ix);
  }
}

void LoRaSender::transmitPayload() {
  if (validEncrypted) {
    Serial.println("LR: == SENDING ==");
    printBytes(currentEncrypted, currentEncryptedLength);

    yield();
    LoRa.beginPacket();
    LoRa.write(currentEncrypted, currentEncryptedLength);
    LoRa.endPacket();
    LoRa.receive();
    yield();
  }
}

bool LoRaSender::checkAcknowledge(uint8_t *ackPackage) {
  Serial.println("LR: ACK Encrypted");
  printBytes(ackPackage, sizeof(Acknowledge));

  Acknowledge unencrypted;
  aesDecrypt.decryptBlock((uint8_t *)&unencrypted, ackPackage);

  Serial.println("LR: ACK Decrypted");
  printBytes((uint8_t *)&unencrypted, sizeof(unencrypted));

  uint8_t ourHash[sizeof(unencrypted.hash)];
  hmacSha256.resetHMAC(mackey, sizeof(mackey));
  hmacSha256.update(((uint8_t *)&unencrypted) + sizeof(unencrypted.hash), sizeof(unencrypted) - sizeof(unencrypted.hash));
  hmacSha256.finalizeHMAC(mackey, sizeof(mackey), ourHash, sizeof(ourHash));

  if (0 != memcmp(unencrypted.hash, ourHash, sizeof(ourHash))) {
    Serial.println("LR: Bad acknowledge HMAC");
    return false;
  }

  if (unencrypted.number != currentPayloadNumber) {
    Serial.println("LR: Bad acknowledge package number");
    return false;
  }

  return true;
}
