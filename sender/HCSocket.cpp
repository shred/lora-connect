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

#include <AES.h>
#include <ArduinoJson.h>
#include <SHA256.h>
#include <WebSocketsClient.h>

#include "CBC.h"  // crypto legacy, local copy
#include "HCSocket.h"
#include "Utils.h"

#define SOCKET_DEBUG false
#define SOCKET_RECONNECT_INTERVAL 5000


HCSocket::HCSocket(const char *base64psk, const char *base64iv, MessageEvent listener) {
  eventListener = listener;

  uint8_t psk[32];
  if (!base64UrlDecode(base64psk, psk, sizeof(psk))) {
    die("HC: psk is invalid, check your config.h!");
  }
  if (!base64UrlDecode(base64iv, iv, sizeof(iv))) {
    die("HC: iv is invalid, check your config.h!");
  }

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
    die("HC: Invalid encryption key");
  }
  if (!aesEncrypt.setIV(iv, aesEncrypt.ivSize())) {
    die("HC: Invalid encryption IV");
  }

  aesDecrypt.clear();
  if (!aesDecrypt.setKey(enckey, aesDecrypt.keySize())) {
    die("HC: Invalid decryption key");
  }
  if (!aesDecrypt.setIV(iv, aesDecrypt.ivSize())) {
    die("HC: Invalid decryption IV");
  }
}

void HCSocket::connect(IPAddress &ip, uint16_t port) {
  this->ip = ip;
  this->port = port;

  reset();

  Serial.printf("HC: Connecting to %s:%u\n", ip.toString(), port);
  webSocket.begin(ip, port, "/homeconnect", "");
  webSocket.onEvent(std::bind(&HCSocket::onWsEvent, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
  webSocket.setReconnectInterval(SOCKET_RECONNECT_INTERVAL);
}

void HCSocket::reconnect() {
  webSocket.disconnect();
  reset();
  webSocket.begin(ip, port, "/homeconnect", "");
}

void HCSocket::send(const JsonDocument &doc) {
#ifdef SOCKET_DEBUG
  Serial.println("HC: TX: Sending JSON message to appliance:");
  serializeJson(doc, Serial);
  Serial.println();
#endif

  // Convert to buffer
  size_t estimatedDocSize = measureJson(doc);
  uint8_t clearMsg[estimatedDocSize + 64];
  size_t docLen = serializeJson(doc, (char *)clearMsg, estimatedDocSize);

  // Add padding for encryption
  size_t padLen = 16 - (docLen % 16);
  if (padLen == 1) {
    padLen += 16;
  }
  size_t messageLen = docLen + padLen;
  if (messageLen > sizeof(cryptBuffer) - sizeof(lastTxHmac)) {
    Serial.printf("HC: TX: Message is too big (%u bytes), dropped!", messageLen);
    return;
  }

  // Fill padding with random numbers
  clearMsg[docLen] = 0;
  for (int ix = docLen + 1; ix < messageLen - 1; ix++) {
    clearMsg[ix] = random(256);
  }
  clearMsg[messageLen - 1] = padLen;

  // Encrypt message
  aesEncrypt.encrypt(cryptBuffer, clearMsg, messageLen);

  // Set HMAC
  hmacSha256.resetHMAC(mackey, sizeof(mackey));
  hmacSha256.update(iv, sizeof(iv));
  hmacSha256.update("E", 1);  // direction
  hmacSha256.update(lastTxHmac, sizeof(lastTxHmac));
  hmacSha256.update(cryptBuffer, messageLen);
  hmacSha256.finalizeHMAC(mackey, sizeof(mackey), lastTxHmac, sizeof(lastTxHmac));
  memcpy(cryptBuffer + messageLen, lastTxHmac, sizeof(lastTxHmac));

  // Send encrypted buffer
  size_t encryptedSize = messageLen + sizeof(lastTxHmac);
  Serial.printf("HC: TX: Sending message, length %u\n", encryptedSize);
  webSocket.sendBIN(cryptBuffer, encryptedSize);
}

void HCSocket::receive(uint8_t *msg, size_t size) {
  Serial.printf("HC: RX: Received message, length %u\n", size);

  // Check if the message size makes sense
  if (size < 32 || size % 16 != 0) {
    Serial.printf("HC: RX: Incomplete message, length %u. Reconnecting!\n", size);
    reconnect();
    return;
  }

  // Check HMAC
  uint8_t ourMac[sizeof(lastRxHmac)];
  hmacSha256.resetHMAC(mackey, sizeof(mackey));
  hmacSha256.update(iv, sizeof(iv));
  hmacSha256.update("C", 1);  // direction
  hmacSha256.update(lastRxHmac, sizeof(lastRxHmac));
  hmacSha256.update(msg, size - 16);
  hmacSha256.finalizeHMAC(mackey, sizeof(mackey), ourMac, sizeof(ourMac));

  if (0 != memcmp(msg + size - 16, ourMac, sizeof(ourMac))) {
    Serial.println("HC: RX: HMAC mismatch. Reconnecting!");
    reconnect();
    return;
  }

  // Remember last HMAC
  memcpy(lastRxHmac, ourMac, sizeof(lastRxHmac));

  // Decrypt message
  size_t decryptedSize = size - 16;
  if (decryptedSize > sizeof(cryptBuffer)) {
    Serial.printf("HC: RX: Message is too big (%u bytes). Reconnecting!", decryptedSize);
    reconnect();
    return;
  }

  aesDecrypt.decrypt(cryptBuffer, msg, decryptedSize);

  // Remove padding
  uint8_t padLen = cryptBuffer[decryptedSize - 1];
  if (padLen > decryptedSize) {
    Serial.println("HC: RX: Padding error. Reconnecting!");
    reconnect();
    return;
  }

  // Parse message
  DynamicJsonDocument doc(decryptedSize * 4);  // Better be generous
  DeserializationError error = deserializeJson(doc, (char *)&cryptBuffer, decryptedSize - padLen);
  if (error) {
    Serial.printf("HC: RX: JSON error, message dropped: %s\n", error.f_str());
    return;
  }

#ifdef SOCKET_DEBUG
  Serial.println("HC: RX: Received JSON message from appliance:");
  serializeJson(doc, Serial);
  Serial.println();
#endif

  eventListener(doc);
}

void HCSocket::startSession(uint32_t sessionId, uint32_t txMsgId) {
  Serial.printf("HC: Starting session, sID=%u, msgID=%u\n", sessionId, txMsgId);
  this->sessionId = sessionId;
  this->txMsgId = txMsgId;
}

void HCSocket::sendActionWithData(const char *resource, const JsonDocument &data, const uint16_t version, const char *action) {
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

void HCSocket::onWsEvent(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println(F("HC: Disconnected from appliance"));
      break;

    case WStype_CONNECTED:
      Serial.println(F("HC: Connected to appliance"));
      reset();
      break;

    case WStype_TEXT:
      Serial.printf("HC: Received unexpected text: %s\n", payload);
      break;

    case WStype_BIN:
#ifdef SOCKET_DEBUG
      Serial.printf("HC: Received message with %u bytes\n", length);
#endif
      receive(payload, length);
      break;

    case WStype_FRAGMENT_TEXT_START:
      Serial.println(F("HC: Received unexpected text fragment"));
      isBinFragment = false;
      break;

    case WStype_FRAGMENT_BIN_START:
#ifdef SOCKET_DEBUG
      Serial.printf("HC: Received start of fragmented message, %u bytes\n", length);
#endif
      fragmentIx = 0;
      isBinFragment = true;
      appendFragment(payload, length);
      break;

    case WStype_FRAGMENT:
#ifdef SOCKET_DEBUG
      Serial.printf("HC: Received fragment, length %u bytes\n", length);
#endif
      appendFragment(payload, length);
      break;

    case WStype_FRAGMENT_FIN:
#ifdef SOCKET_DEBUG
      Serial.printf("HC: Received fragment end, length %u bytes\n", length);
#endif
      appendFragment(payload, length);
      if (isBinFragment && fragmentIx > 0) {
        receive(fragment, fragmentIx);
        fragmentIx = 0;
        isBinFragment = false;
      }
      break;

    case WStype_ERROR:
      Serial.printf("HC: web socket error %u\n", length);
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
    Serial.println(F("HC: Fragment buffer overflow, fragment part was dropped!"));
  }
}
