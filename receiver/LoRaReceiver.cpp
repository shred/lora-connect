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
#include "LoRaReceiver.h"
#include "base64url.h"

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

LoRaReceiver::LoRaReceiver(const char *base64key) {
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

  lastMessageNumber = 0;
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
  if (packetSize == 0) {
    Serial.println("LR: Empty message, ignoring");
    return;
  }

  if (packetSize > sizeof(Payload) || packetSize % 16 != 0) {
    Serial.printf("LR: Unexpected message length %u, ignoring\n", packetSize);
    return;
  }

  uint8_t cryptBuffer[packetSize];
  size_t receiveLength = 0;
  while (LoRa.available()) {
    if (receiveLength > sizeof(cryptBuffer)) {
      Serial.println("LR: Message exceeds buffer, ignoring");
      return;
    }
    cryptBuffer[receiveLength++] = (uint8_t)LoRa.read();
  }

  if (receiveLength != sizeof(cryptBuffer)) {
    Serial.printf("LR: Expected message length was %u, but is %u\n", sizeof(cryptBuffer), receiveLength);
    return;
  }

  Serial.println("LR: == RECEIVED ==");
  printBytes(cryptBuffer, receiveLength);

  // Decrypt
  uint8_t *clearBuffer = (uint8_t *)&payload;
  size_t blockSize = aesEncrypt.blockSize();
  for (int ix = 0; ix < sizeof(cryptBuffer); ix += blockSize) {
    aesDecrypt.decryptBlock(clearBuffer + ix, cryptBuffer + ix);
  }

  Serial.println("LR: Decrypted");
  printBytes(clearBuffer, sizeof(payload));

  uint8_t theirHash[sizeof(payload.hash)];
  memcpy(theirHash, payload.hash, sizeof(theirHash));
  memset(payload.hash, 0, sizeof(payload.hash));

  uint8_t ourHash[sizeof(payload.hash)];
  hmacSha256.resetHMAC(mackey, sizeof(mackey));
  hmacSha256.update(&payload, receiveLength);
  hmacSha256.finalizeHMAC(mackey, sizeof(mackey), ourHash, sizeof(ourHash));

  if (0 != memcmp(theirHash, ourHash, sizeof(ourHash))) {
    Serial.println("LR: Bad HMAC, maybe not our package");
    return;
  }

  // Send Acknowledge
  Acknowledge acknowledge;
  acknowledge.number = payload.number;
  memset(acknowledge.hash, 0, sizeof(acknowledge.hash));
  for (int ix = 0; ix < sizeof(acknowledge.pad); ix++) {
    acknowledge.pad[ix] = random(256);
  }

  uint8_t ackHash[sizeof(acknowledge.hash)];
  hmacSha256.resetHMAC(mackey, sizeof(mackey));
  hmacSha256.update(&acknowledge, sizeof(acknowledge));
  hmacSha256.finalizeHMAC(mackey, sizeof(mackey), ackHash, sizeof(ackHash));
  memcpy(acknowledge.hash, ackHash, sizeof(acknowledge.hash));

  Serial.println("LR: Ack unencrypted");
  printBytes((uint8_t *)&acknowledge, sizeof(acknowledge));

  uint8_t ackEncrypted[sizeof(acknowledge)];
  aesEncrypt.encryptBlock(ackEncrypted, (uint8_t *)&acknowledge);

  Serial.println("LR: Ack encrypted");
  printBytes((uint8_t *)&ackEncrypted, sizeof(acknowledge));

  yield();
  delay(100);
  LoRa.beginPacket();
  LoRa.setTxPower(LORA_POWER, (LORA_PABOOST == true ? RF_PACONFIG_PASELECT_PABOOST : RF_PACONFIG_PASELECT_RFO));
  LoRa.write(ackEncrypted, sizeof(ackEncrypted));
  LoRa.endPacket();
  LoRa.flush();
  yield();

  if (payload.number == lastMessageNumber) {
    Serial.println("LR: Message already received, maybe ACK was lost, ignoring");
    return;
  }

  lastMessageNumber = payload.number;

  cursor = 0;
  while (cursor < payload.length) {
    uint8_t type = payload.data[cursor++];
    switch (type) {
      case 0:  // int, constant zero
        {
          uint16_t key = readKey();
          if (intEventListener) {
            intEventListener(key, 0);
          }
        }

      case 1:  // uint8_t positive
      case 2:  // uint8_t negative
        {
          uint16_t key = readKey();
          int32_t value = readInteger(1, type == 2);
          if (intEventListener) {
            intEventListener(key, value);
          }
        }
        break;

      case 3:  // uint16_t positive
      case 4:  // uint16_t negative
        {
          uint16_t key = readKey();
          int32_t value = readInteger(2, type == 4);
          if (intEventListener) {
            intEventListener(key, value);
          }
        }
        break;

      case 5:  // uint32_t positive
      case 6:  // uint32_t negative
        {
          uint16_t key = readKey();
          int32_t value = readInteger(4, type == 6);
          if (intEventListener) {
            intEventListener(key, value);
          }
        }
        break;

      case 7:  // boolean "false"
      case 8:  // boolean "true"
        {
          uint16_t key = readKey();
          if (booleanEventListener) {
            booleanEventListener(key, type == 8);
          }
        }
        break;

      case 9:  // String
        {
          uint16_t key = readKey();
          String str = readString();
          if (stringEventListener) {
            stringEventListener(key, str);
          }
        }
        break;

      case 255:  // System message
        {
          String str = readString();
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

uint16_t LoRaReceiver::readKey() {
  uint16_t result = 0;
  if (cursor + 2 <= payload.length) {
    result = (payload.data[cursor] & 0xFF) | ((payload.data[cursor + 1] & 0xFF) << 8);
    cursor += 2;
  }
  return result;
}

int32_t LoRaReceiver::readInteger(size_t len, bool neg) {
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

String LoRaReceiver::readString() {
  size_t strlen = 0;
  uint8_t *start = payload.data + cursor;
  while (payload.data[cursor] != 0 && cursor < payload.length) {
    strlen++;
    cursor++;
  }
  cursor++;  // Also skip null terminator
  return String(start, strlen);
}

void LoRaReceiver::loop() {
  int packetSize = LoRa.parsePacket();
  yield();
  if (packetSize) {
    onLoRaReceive(packetSize);
  }
}
