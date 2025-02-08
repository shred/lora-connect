/*
 * LoRa-Connect
 *
 * Copyright (C) 2023 Richard "Shred" Körber
 *   https://codeberg.org/shred/lora-connect
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
#include <LoRa.h>
#include <SPI.h>

#include "LoRaSender.h"
#include "Utils.h"

#include "config.h"


// pins of the Heltec LoRa32 V2 transceiver module
#define LORA_SCK 5
#define LORA_MISO 19
#define LORA_MOSI 27
#define LORA_CS 18
#define LORA_RST 14
#define LORA_DIO0 26
#define LORA_DIO1 35
#define LORA_DIO2 34

#define LORA_PACKAGE_RATE_LIMIT 1000
#define LORA_ACK_TIMEOUT 1000


LoRaSender::LoRaSender(const char *base64key) {
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);

  lastSendTime = millis();
  lastPushTime = millis();
  nextSendDelay = 0;

  uint8_t key[32];
  if (!base64UrlDecode(base64key, key, sizeof(key))) {
    die("LR: key is invalid, check your config.h!");
  }

  hmacSha256.resetHMAC(key, sizeof(key));
  hmacSha256.update("LORAENC", 7);
  hmacSha256.finalizeHMAC(key, sizeof(key), enckey, sizeof(enckey));

  hmacSha256.resetHMAC(key, sizeof(key));
  hmacSha256.update("LORAMAC", 7);
  hmacSha256.finalizeHMAC(key, sizeof(key), mackey, sizeof(mackey));

  aesEncrypt.clear();
  if (!aesEncrypt.setKey(enckey, aesEncrypt.keySize())) {
    die("LR: Invalid encryption key");
  }
  aesDecrypt.clear();
  if (!aesDecrypt.setKey(enckey, aesEncrypt.keySize())) {
    die("LR: Invalid decryption key");
  }

  validEncrypted = false;
  senderQueue = new cppQueue(sizeof(Payload), PAYLOAD_BUFFER_SIZE);
  acknowledgeQueue = new cppQueue(sizeof(Acknowledge), PAYLOAD_BUFFER_SIZE);
}

LoRaSender::~LoRaSender() {
  LoRa.end();
  delete acknowledgeQueue;
  delete senderQueue;
}

void LoRaSender::connect() {
  if (!LoRa.begin(LORA_BAND)) {
    die("LR: Failed to start LoRa");
  }

  LoRa.setTxPower(LORA_POWER, LORA_PABOOST ? PA_OUTPUT_PA_BOOST_PIN : PA_OUTPUT_RFO_PIN);
  LoRa.setSpreadingFactor(LORA_SPREADING);
  LoRa.setSignalBandwidth(LORA_BANDWIDTH);
  LoRa.setSyncWord(LORA_SYNCWORD);
}

void LoRaSender::sendInt(uint16_t key, int32_t value) {
  Serial.printf("LR: sending int %u = %d\n", key, value);

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
  Serial.printf("LR: sending bool %u = %d\n", key, value);
  sendMessage(value ? 8 : 7, key, NULL, 0);
}

void LoRaSender::sendString(uint16_t key, String value) {
  Serial.printf("LR: sending string %u = '%s'\n", key, value.c_str());
  sendMessage(9, key, (uint8_t *)value.c_str(), value.length() + 1);
}

void LoRaSender::sendSystemMessage(String message) {
  Serial.printf("LR: sending system msg '%s'\n", message.c_str());

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

  // System messages are sent immediately
  flush();
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

  lastPushTime = millis();
}

void LoRaSender::flush() {
  if (payloadBuffer.length != 0) {
    sendRaw(payloadBuffer);
    payloadBuffer.number++;
    payloadBuffer.length = 0;
    lastPushTime = millis();
  }
}

void LoRaSender::sleep() {
  Serial.println("LR: Put LoRa to sleep");
  LoRa.idle();
}

void LoRaSender::sendRaw(Payload &payload) {
  if (!senderQueue->push(&payload)) {
    Serial.println("LR: Queue is full, payload was dropped!");
  }
}

void LoRaSender::onLoRaReceive(int packetSize) {
  if (packetSize != sizeof(Acknowledge)) {
    Serial.printf("LRC: Ignoring message with length %u\n", packetSize);
    return;
  }

  uint8_t cryptBuffer[sizeof(Acknowledge)];

  size_t receiveLength = 0;
  uint8_t chr;
  while (LoRa.available()) {
    chr = (uint8_t)LoRa.read();
    if (receiveLength < sizeof(cryptBuffer)) {
      cryptBuffer[receiveLength++] = chr;
    }
  }

  if (acknowledgeQueue->push(&cryptBuffer)) {
    Serial.println("LRC: Received acknowledge message");
  } else {
    Serial.println("LRC: Queue is full, message was dropped!");
  }
}

void LoRaSender::loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    onLoRaReceive(packetSize);
  }

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

#ifdef LORA_COLLECT_TIME
  if (!validEncrypted && payloadBuffer.length != 0 && (millis() - lastPushTime) > LORA_COLLECT_TIME) {
    flush();
  }
  yield();
#endif

  if (validEncrypted) {
    if ((millis() - lastSendTime) > nextSendDelay) {
      attempts++;
      if (attempts <= LORA_MAX_SENDING_ATTEMPTS) {
        Serial.printf("LR: Transmitting %u bytes (attempt %u/%u)\n", currentEncryptedLength, attempts, LORA_MAX_SENDING_ATTEMPTS);
        transmitPayload();
        lastSendTime = millis();
        nextSendDelay = LORA_PACKAGE_RATE_LIMIT + random(100);
      } else {
        Serial.println("LR: Maximum number of reattempts reached, package dropped!");
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

  // Encrypt
  const uint8_t *clearBuffer = (const uint8_t *)&sendPayload;
  size_t blockSize = aesEncrypt.blockSize();
  for (int ix = 0; ix < currentEncryptedLength; ix += blockSize) {
    aesEncrypt.encryptBlock(currentEncrypted + ix, clearBuffer + ix);
  }
}

void LoRaSender::transmitPayload() {
  if (validEncrypted) {
    LoRa.beginPacket();
    LoRa.write(currentEncrypted, currentEncryptedLength);
    LoRa.endPacket();
    yield();
  }
}

bool LoRaSender::checkAcknowledge(uint8_t *ackPackage) {
  // Decrypt acknowledge message
  Acknowledge unencrypted;
  aesDecrypt.decryptBlock((uint8_t *)&unencrypted, ackPackage);

  // Check the hash
  uint8_t ourHash[sizeof(unencrypted.hash)];
  hmacSha256.resetHMAC(mackey, sizeof(mackey));
  hmacSha256.update(((uint8_t *)&unencrypted) + sizeof(unencrypted.hash), sizeof(unencrypted) - sizeof(unencrypted.hash));
  hmacSha256.finalizeHMAC(mackey, sizeof(mackey), ourHash, sizeof(ourHash));

  if (0 != memcmp(unencrypted.hash, ourHash, sizeof(ourHash))) {
    Serial.println("LR: Bad acknowledge HMAC, ignoring");
    return false;
  }

  if (unencrypted.number != currentPayloadNumber) {
    Serial.println("LR: Unexpected package number, ignoring");
    return false;
  }

  return true;
}
