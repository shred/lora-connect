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

#ifndef __HCSocket__
#define __HCSocket__

#include <AES.h>
#include <ArduinoJson.h>
#include <SHA256.h>
#include <WebSocketsClient.h>
#include "CBC.h"  // crypto legacy, local copy

/**
 * Socket for connecting to Home Connect appliances.
 */
class HCSocket {
  using MessageEvent = void (*)(const JsonDocument &message);

public:

  /**
   * Set up the socket with the encryption keys to be used. The given MessageEvent
   * listener is invoked when a message from the appliance was received.
   */
  HCSocket(const char *base64key, const char *base64iv, MessageEvent listener);

  /**
   * Open a connection to the appliance.
   */
  void connect(IPAddress &ip, uint16_t port);

  /**
   * Close the connection, then reconnect. This is useful to bring the socket
   * back to a defined state, e.g. after a transmission error.
   */
  void reconnect();

  /**
   * Reset the connection state, e.g. after reconnecting. All session parameters
   * are reset to the initial state.
   */
  void reset();

  /**
   * Encrypt and send a message to the appliance.
   */
  void send(const JsonDocument &doc);

  /**
   * Decrypt and parse a received message from the appliance.
   */
  void receive(uint8_t *msg, size_t size);

  /**
   * Start a session after establishing a connection.
   */
  void startSession(uint32_t sessionId, uint32_t txMsgId);

  /**
   * Send an action request.
   */
  void sendActionWithData(const char *resource, const JsonDocument &data, const uint16_t version = 1, const char *action = "GET");

  /**
   * Send an action request, without payload.
   */
  void sendAction(const char *resource, const uint16_t version = 1, const char *action = "GET");

  /**
   * Send an action request.
   */
  void sendReply(const JsonDocument &query, const JsonDocument &reply);

  /**
   * Creates a random nonce that is required by some appliances.
   */
  String createRandomNonce();

  /**
   * Must be invoked in the .ino loop function.
   */
  void loop();

private:
  uint8_t psk[32];
  uint8_t iv[16];

  uint8_t enckey[SHA256::HASH_SIZE];
  uint8_t mackey[SHA256::HASH_SIZE];

  uint8_t lastRxHmac[16];
  uint8_t lastTxHmac[16];

  IPAddress ip;
  uint16_t port;

  CBC<AES256> aesEncrypt;
  CBC<AES256> aesDecrypt;
  SHA256 hmacSha256;

  uint32_t sessionId;
  uint32_t txMsgId;

  MessageEvent eventListener;
  WebSocketsClient webSocket;

  uint8_t fragment[32768];
  uint16_t fragmentIx;
  bool isBinFragment;

  void appendFragment(uint8_t *payload, size_t length);
  void onWsEvent(WStype_t type, uint8_t *payload, size_t length);
};

#endif