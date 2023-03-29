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

#define BASE64_URL  // set base64.hpp to base64url mode

#define SOCKET_DEBUG

#include <AES.h>
#include <ArduinoJson.h>
#include <base64.hpp>
#include <SHA256.h>
#include <WebSocketsClient.h>

#include "CBC.h"  // crypto legacy, local copy
#include "HCSocket.h"

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


HCSocket::HCSocket(const char *base64psk, const char *base64iv, MessageEvent listener) {
  eventListener = listener;

  if (decode_base64_length((unsigned char *)base64psk) != 32) {
    throw std::invalid_argument("Invalid key length");
  }
  decode_base64((unsigned char *)base64psk, psk);

  if (decode_base64_length((unsigned char *)base64iv) != 16) {
    throw std::invalid_argument("Invalid IV length");
  }
  decode_base64((unsigned char *)base64iv, iv);

  hmacSha256.resetHMAC(psk, sizeof(psk));
  hmacSha256.update("ENC", 3);
  hmacSha256.finalizeHMAC(psk, sizeof(psk), enckey, sizeof(enckey));

  hmacSha256.resetHMAC(psk, sizeof(psk));
  hmacSha256.update("MAC", 3);
  hmacSha256.finalizeHMAC(psk, sizeof(psk), mackey, sizeof(mackey));
}

void HCSocket::loop() {
  webSocket.loop();
}

void HCSocket::reset() {
  sessionId = 0;
  txMsgId = 0;
  fragmentIx = 0;

  memset(lastRxHmac, 0, sizeof(lastRxHmac));
  memset(lastTxHmac, 0, sizeof(lastTxHmac));

  aesEncrypt.clear();
  if (!aesEncrypt.setKey(enckey, aesEncrypt.keySize())) {
    throw std::invalid_argument("Invalid key");
  }
  if (!aesEncrypt.setIV(iv, aesEncrypt.ivSize())) {
    throw std::invalid_argument("Invalid IV");
  }

  aesDecrypt.clear();
  if (!aesDecrypt.setKey(enckey, aesDecrypt.keySize())) {
    throw std::invalid_argument("Invalid key");
  }
  if (!aesDecrypt.setIV(iv, aesDecrypt.ivSize())) {
    throw std::invalid_argument("Invalid IV");
  }
}

void HCSocket::connect(IPAddress &ip, uint16_t port) {
  this->ip = ip;
  this->port = port;

  reset();

  Serial.printf("Connecting to %s port %u\n", ip.toString(), port);
  webSocket.begin(ip, port, "/homeconnect", "");
  webSocket.onEvent(std::bind(&HCSocket::onWsEvent, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
  webSocket.setReconnectInterval(5000);
}

void HCSocket::reconnect() {
  webSocket.disconnect();
  reset();
  webSocket.begin(ip, port, "/homeconnect", "");
}

void HCSocket::send(const JsonDocument &doc) {
  #ifdef SOCKET_DEBUG
    Serial.print("TX: ");
    serializeJson(doc, Serial);
    Serial.println();
  #endif

  size_t docSize = measureJson(doc);
  uint8_t clearMsg[docSize + 64];
  size_t docLen = serializeJson(doc, (char *)clearMsg, docSize);

  size_t padLen = 16 - (docLen % 16);
  if (padLen == 1) {
    padLen += 16;
  }
  size_t messageLen = docLen + padLen;

  clearMsg[docLen] = 0;
  for (int ix = docLen + 1; ix < messageLen - 1; ix++) {
    clearMsg[ix] = random(256);
  }
  clearMsg[messageLen - 1] = padLen;

  uint8_t encryptedMsg[messageLen + sizeof(lastTxHmac)];
  aesEncrypt.encrypt(encryptedMsg, clearMsg, messageLen);

  hmacSha256.resetHMAC(mackey, sizeof(mackey));
  hmacSha256.update(iv, sizeof(iv));
  hmacSha256.update("E", 1);  // direction
  hmacSha256.update(lastTxHmac, sizeof(lastTxHmac));
  hmacSha256.update(encryptedMsg, messageLen);
  hmacSha256.finalizeHMAC(mackey, sizeof(mackey), lastTxHmac, sizeof(lastTxHmac));
  memcpy(encryptedMsg + messageLen, lastTxHmac, sizeof(lastTxHmac));

  #ifdef SOCKET_DEBUG
    Serial.println(F("TX: Encrypted"));
    printBytes(encryptedMsg, sizeof(encryptedMsg));
  #endif
  webSocket.sendBIN(encryptedMsg, sizeof(encryptedMsg));
}

void HCSocket::receive(uint8_t *msg, size_t size) {
  if (size < 32 || size % 16 != 0) {
    Serial.printf("RX: Incomplete message, length %u\n", size);
    reconnect();
    return;
  }

  #ifdef SOCKET_DEBUG
    Serial.println("RX: Encrypted");
    printBytes(msg, size);
  #endif

  uint8_t ourMac[sizeof(lastRxHmac)];
  hmacSha256.resetHMAC(mackey, sizeof(mackey));
  hmacSha256.update(iv, sizeof(iv));
  hmacSha256.update("C", 1);  // direction
  hmacSha256.update(lastRxHmac, sizeof(lastRxHmac));
  hmacSha256.update(msg, size - 16);
  hmacSha256.finalizeHMAC(mackey, sizeof(mackey), ourMac, sizeof(ourMac));

  #ifdef SOCKET_DEBUG
    Serial.println("RX: Our MAC");
    printBytes(ourMac, sizeof(ourMac));
    Serial.println("RX: Their MAC");
    printBytes(msg + size - 16, sizeof(ourMac));
  #endif

  if (0 != memcmp(msg + size - 16, ourMac, sizeof(ourMac))) {
    Serial.println("RX: Bad HMAC, a message was lost");
    reconnect();
    return;
  }

  memcpy(lastRxHmac, ourMac, sizeof(lastRxHmac));

  uint8_t decryptedMsg[size - 16];
  aesDecrypt.decrypt(decryptedMsg, msg, sizeof(decryptedMsg));

  uint8_t padLen = decryptedMsg[sizeof(decryptedMsg) - 1];
  if (padLen > sizeof(decryptedMsg)) {
    Serial.println("RX: Padding error");
    reconnect();
    return;
  }

  #ifdef SOCKET_DEBUG
    Serial.println("RX: Raw message");
    printBytes(decryptedMsg, sizeof(decryptedMsg));
  #endif

  DynamicJsonDocument doc(sizeof(decryptedMsg) * 4);  // Better be generous

  DeserializationError error = deserializeJson(doc, (const char *)&decryptedMsg, sizeof(decryptedMsg) - padLen);
  if (error) {
    Serial.printf("RX: JSON error %s\n", error.f_str());
    return;
  }

  #ifdef SOCKET_DEBUG
    Serial.println("RX:");
    serializeJson(doc, Serial);
    Serial.println();
  #endif

  eventListener(doc);
}

void HCSocket::startSession(uint32_t sessionId, uint32_t txMsgId) {
  #ifdef SOCKET_DEBUG
    Serial.printf("Starting session, sID=%u, msgID=%u\n", sessionId, txMsgId);
  #endif
  this->sessionId = sessionId;
  this->txMsgId = txMsgId;
}

void HCSocket::sendActionWithData(const char *resource, const JsonDocument &data, const uint16_t version, const char *action) {
  #ifdef SOCKET_DEBUG
    Serial.printf("Sending action %s to resource %s\n", action, resource);
  #endif

  DynamicJsonDocument doc(1024);
  doc["sID"] = sessionId;
  doc["msgID"] = txMsgId;
  doc["resource"] = resource;
  doc["version"] = version;
  doc["action"] = action;
  if (!data.isNull()) {
    doc.createNestedArray("data").add(data);  // stores by copy
  }
  send(doc);

  txMsgId++;
}

void HCSocket::sendAction(const char *resource, const uint16_t version, const char *action) {
  StaticJsonDocument<10> empty;
  sendActionWithData(resource, empty, version, action);
}

void HCSocket::sendReply(const JsonDocument &query, const JsonDocument &reply) {
  #ifdef SOCKET_DEBUG
    Serial.printf("Sending reply to query msgId=%u\n", query["msgID"]);
  #endif

  DynamicJsonDocument doc(1024);
  doc["sID"] = query["sID"];
  doc["msgID"] = query["msgID"];
  doc["resource"] = query["resource"];
  doc["version"] = query["version"];
  doc["action"] = "RESPONSE";
  if (!reply.isNull()) {
    doc.createNestedArray("data").add(reply);  // stores by copy
  }
  send(doc);
}

String HCSocket::createRandomNonce() {
  uint8_t tokenBin[32];
  for (int ix = 0; ix < sizeof(tokenBin); ix++) {
    tokenBin[ix] = random(256);
  }

  uint8_t encodedToken[48];
  size_t len = encode_base64(tokenBin, sizeof(tokenBin), encodedToken);
  while (len > 0 && encodedToken[len - 1] == '=') {
    encodedToken[len - 1] = 0;
    len--;
  }

  return String((char *)&encodedToken);
}

void HCSocket::onWsEvent(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println(F("WS disconnected"));
      break;

    case WStype_CONNECTED:
      Serial.println(F("WS connected"));
      reset();
      break;

    case WStype_TEXT:
      Serial.printf("WS unexpected text: %s\n", payload);
      break;

    case WStype_BIN:
      Serial.printf("WS received message with %u bytes\n", length);
      receive(payload, length);
      break;

    case WStype_FRAGMENT_TEXT_START:
      Serial.printf("WS unexpected text fragment start, length %u bytes\n", length);
      isBinFragment = false;
      break;

    case WStype_FRAGMENT_BIN_START:
      Serial.printf("WS Bin fragment start, length %u bytes\n", length);
      fragmentIx = 0;
      isBinFragment = true;
      appendFragment(payload, length);
      break;

    case WStype_FRAGMENT:
      Serial.printf("WS fragment, length %u bytes\n", length);
      appendFragment(payload, length);
      break;

    case WStype_FRAGMENT_FIN:
      Serial.printf("WS fragment fin, length %u bytes\n", length);
      appendFragment(payload, length);
      if (isBinFragment && fragmentIx > 0) {
        receive(fragment, fragmentIx);
        fragmentIx = 0;
        isBinFragment = false;
      }
      break;

    case WStype_ERROR:
      Serial.printf("WS error %u\n", length);
      break;

    case WStype_PING:
      Serial.println(F("WS ping"));
      break;

    case WStype_PONG:
      Serial.println(F("WS pong"));
      break;
  }
}

void HCSocket::appendFragment(uint8_t *payload, size_t length) {
  if (!isBinFragment) {
    return;
  }
  if (fragmentIx + length < sizeof(fragment)) {
    memcpy(fragment + fragmentIx, payload, length);
    fragmentIx += length;
  } else {
    Serial.println(F("WS fragment buffer overflow!"));
  }
}
