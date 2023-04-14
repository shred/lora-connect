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

#include <LoRa.h>
#include <SPI.h>

#include "LoRaReceiver.h"
#include "Utils.h"

#include "config.h"

// Pins of the Heltec LoRa32 V2 transceiver module.
#define LORA_SCK 5
#define LORA_MISO 19
#define LORA_MOSI 27
#define LORA_CS 18
#define LORA_RST 14
#define LORA_DIO0 26
#define LORA_DIO1 35
#define LORA_DIO2 34


static uint16_t readKey(Payload &payload, uint8_t &cursor) {
  uint16_t result = 0;
  if (cursor + 2 <= payload.length) {
    result = (payload.data[cursor] & 0xFF) | ((payload.data[cursor + 1] & 0xFF) << 8);
    cursor += 2;
  }
  return result;
}

static int32_t readInteger(Payload &payload, uint8_t &cursor, size_t len, bool neg) {
  int32_t result = 0;
  if (cursor + len <= payload.length) {
    for (int pos = len - 1; pos >= 0; pos--) {
      result <<= 8;
      result |= payload.data[cursor + pos] & 0xFF;
    }
    cursor += len;
  }
  if (neg) {
    result = -result;
  }
  return result;
}

static String readString(Payload &payload, uint8_t &cursor) {
  size_t strlen = 0;
  uint8_t *start = payload.data + cursor;
  while (payload.data[cursor] != 0 && cursor < payload.length) {
    strlen++;
    cursor++;
  }
  cursor++;  // Also skip null terminator
  return String(start, strlen);
}

LoRaReceiver::LoRaReceiver(const char *base64key) {
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);

  uint8_t key[32];
  if (!base64UrlDecode(base64key, key, sizeof(key))) {
    die("LR: Encryption key is invalid, check your config.h!");
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

  receiverQueue = new cppQueue(sizeof(Encrypted), PAYLOAD_BUFFER_SIZE);

  lastMessageNumber = 0;
}

LoRaReceiver::~LoRaReceiver() {
  LoRa.end();
  delete receiverQueue;
}

void LoRaReceiver::connect() {
  if (!LoRa.begin(LORA_BAND)) {
    die("LR: Failed to initialize LoRa!");
  }

  LoRa.setTxPower(LORA_POWER, LORA_PABOOST ? PA_OUTPUT_PA_BOOST_PIN : PA_OUTPUT_RFO_PIN);
  LoRa.setSpreadingFactor(LORA_SPREADING);
  LoRa.setSignalBandwidth(LORA_BANDWIDTH);
  LoRa.setSyncWord(LORA_SYNCWORD);
}

void LoRaReceiver::loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    onLoRaReceive(packetSize);
  }
  yield();

  Encrypted receivedMessage;
  if (receiverQueue->pop(&receivedMessage)) {
    Payload receivedPayload;
    if (decryptMessage(receivedMessage, receivedPayload)) {
      processPayload(receivedPayload);
    }
  }
  yield();
}

void LoRaReceiver::onLoRaReceive(int packetSize) {
  if (packetSize == 0 || packetSize > sizeof(Payload) || packetSize % 16 != 0) {
    Serial.printf("LRC: Ignoring message with length %u\n", packetSize);
    return;
  }

  Encrypted cryptBuffer;
  cryptBuffer.length = packetSize;

  size_t receiveLength = 0;
  uint8_t chr;
  while (LoRa.available()) {
    chr = (uint8_t)LoRa.read();
    if (receiveLength < sizeof(cryptBuffer.payload)) {
      cryptBuffer.payload[receiveLength++] = chr;
    }
  }

  if (receiverQueue->push(&cryptBuffer)) {
    Serial.printf("LRC: Received message with length %u\n", packetSize);
  } else {
    Serial.println("LRC: Queue is full, message was dropped!");
  }
}

bool LoRaReceiver::decryptMessage(Encrypted &encrypted, Payload &payload) {
  // Decrypt
  uint8_t *clearBuffer = (uint8_t *)&payload;
  size_t blockSize = aesEncrypt.blockSize();
  for (int ix = 0; ix < encrypted.length; ix += blockSize) {
    aesDecrypt.decryptBlock(clearBuffer + ix, encrypted.payload + ix);
  }

  // Compare hashes
  uint8_t ourHash[sizeof(payload.hash)];
  hmacSha256.resetHMAC(mackey, sizeof(mackey));
  hmacSha256.update(((uint8_t *)&payload) + sizeof(payload.hash), encrypted.length - sizeof(payload.hash));
  hmacSha256.finalizeHMAC(mackey, sizeof(mackey), ourHash, sizeof(ourHash));

  if (0 != memcmp(payload.hash, ourHash, sizeof(ourHash))) {
    Serial.println("LR: Bad HMAC");
    return false;
  }

  // Send acknowledge
  sendAck(payload.number);

  // Check for duplicate
  if (payload.number == lastMessageNumber) {
    Serial.println("LR: Message already received");
    return false;
  }
  lastMessageNumber = payload.number;

  return true;
}

void LoRaReceiver::sendAck(uint16_t messageId) {
  Acknowledge acknowledge;
  acknowledge.number = messageId;

  // Fill padding with random bytes
  for (int ix = 0; ix < sizeof(acknowledge.pad); ix++) {
    acknowledge.pad[ix] = random(256);
  }

  // Calculate hash
  hmacSha256.resetHMAC(mackey, sizeof(mackey));
  hmacSha256.update(((uint8_t *)&acknowledge) + sizeof(acknowledge.hash), sizeof(acknowledge) - sizeof(acknowledge.hash));
  hmacSha256.finalizeHMAC(mackey, sizeof(mackey), acknowledge.hash, sizeof(acknowledge.hash));

  // Encrypt
  uint8_t ackEncrypted[sizeof(acknowledge)];
  aesEncrypt.encryptBlock(ackEncrypted, (uint8_t *)&acknowledge);

  // Send
  LoRa.beginPacket();
  LoRa.write(ackEncrypted, sizeof(ackEncrypted));
  LoRa.endPacket();
  yield();
}

void LoRaReceiver::processPayload(Payload &payload) {
  uint8_t cursor = 0;
  while (cursor < payload.length) {
    uint8_t type = payload.data[cursor++];
    switch (type) {
      case 0:  // int, constant zero
        {
          uint16_t key = readKey(payload, cursor);
          if (intEventListener) {
            intEventListener(key, 0);
          }
        }
        break;

      case 1:  // uint8_t positive
      case 2:  // uint8_t negative
        {
          uint16_t key = readKey(payload, cursor);
          int32_t value = readInteger(payload, cursor, 1, type == 2);
          if (intEventListener) {
            intEventListener(key, value);
          }
        }
        break;

      case 3:  // uint16_t positive
      case 4:  // uint16_t negative
        {
          uint16_t key = readKey(payload, cursor);
          int32_t value = readInteger(payload, cursor, 2, type == 4);
          if (intEventListener) {
            intEventListener(key, value);
          }
        }
        break;

      case 5:  // uint32_t positive
      case 6:  // uint32_t negative
        {
          uint16_t key = readKey(payload, cursor);
          int32_t value = readInteger(payload, cursor, 4, type == 6);
          if (intEventListener) {
            intEventListener(key, value);
          }
        }
        break;

      case 7:  // boolean "false"
      case 8:  // boolean "true"
        {
          uint16_t key = readKey(payload, cursor);
          if (booleanEventListener) {
            booleanEventListener(key, type == 8);
          }
        }
        break;

      case 9:  // String
        {
          uint16_t key = readKey(payload, cursor);
          String str = readString(payload, cursor);
          if (stringEventListener) {
            stringEventListener(key, str);
          }
        }
        break;

      case 255:  // System message
        {
          String str = readString(payload, cursor);
          if (systemMessageEventListener) {
            systemMessageEventListener(str);
          }
        }
        break;

      default:
        Serial.printf("LR: Unknown message type %u, ignoring rest of message\n", type);
        return;
    }
  }
}

int LoRaReceiver::getRssi() {
  return LoRa.rssi();
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
