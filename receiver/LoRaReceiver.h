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

#ifndef __LoRaReceiver__
#define __LoRaReceiver__

#include <AES.h>
#include <Arduino.h>
#include <assert.h>
#include <cppQueue.h>
#include <SHA256.h>


// Must be a multiple of 16. In the European Union, the maximum permitted LoRa
// payload size over all data rates is 51 bytes, so the next smaller payload
// size is 48. LoRaSender sends the shortest possible payload with a length
// that is divisible by 16.
#define MAX_PAYLOAD_SIZE 48

// Size of the acknowledge package, must be a multiple of 16.
#define MAX_ACK_SIZE 16

// Maximum number of payloads to keep in the buffer.
#define PAYLOAD_BUFFER_SIZE 32


typedef struct payload {
  uint8_t hash[4];  // MUST be the first element!
  uint16_t number;
  uint8_t length;
  uint8_t data[MAX_PAYLOAD_SIZE - sizeof(hash) - sizeof(number) - sizeof(length)];
} Payload;
static_assert(sizeof(struct payload) == MAX_PAYLOAD_SIZE, "payload structure does not have expected size");

typedef struct encrypted {
  uint8_t payload[sizeof(Payload)];
  size_t length;
} Encrypted;

typedef struct acknowledge {
  uint8_t hash[4];  // MUST be the first element!
  uint16_t number;
  uint8_t pad[MAX_ACK_SIZE - sizeof(hash) - sizeof(number)];
} Acknowledge;
static_assert(sizeof(struct acknowledge) == MAX_ACK_SIZE, "acknowledge structure does not have expected size");

/**
 * LoRa Connection
 */
class LoRaReceiver {
  using ReceiveIntEvent = void (*)(const uint16_t key, const int32_t value);
  using ReceiveBooleanEvent = void (*)(const uint16_t key, const bool value);
  using ReceiveStringEvent = void (*)(const uint16_t key, const String value);
  using ReceiveSystemMessageEvent = void (*)(const String value);

public:
  /**
   * Constuctor.
   */
  LoRaReceiver(const char *base64key);

  /**
   * Destructor.
   */
  ~LoRaReceiver();

  /**
   * Start LoRa connection after everything is set up.
   */
  void connect();

  /**
   * Callback when an integer was received.
   */
  void onReceiveInt(ReceiveIntEvent intEventListener);

  /**
   * Callback when a boolean was received.
   */
  void onReceiveBoolean(ReceiveBooleanEvent booleanEventListener);

  /**
   * Callback when a String was received.
   */
  void onReceiveString(ReceiveStringEvent stringEventListener);

  /**
   * Callback when a general system message was received.
   */
  void onReceiveSystemMessage(ReceiveSystemMessageEvent systemMessageEventListener);

  /**
   * Invoked in main loop.
   */
  void loop();

  /**
   * Return the current RSSI.
   */
  int getRssi();

private:
  void onLoRaReceive(int packetSize);
  void processMessage(Encrypted &payload);
  void processPayload(Payload &payload);
  void sendAck(uint16_t messageId);

  uint16_t lastMessageNumber;

  uint8_t enckey[SHA256::HASH_SIZE];
  uint8_t mackey[SHA256::HASH_SIZE];
  AES256 aesEncrypt;
  AES256 aesDecrypt;
  SHA256 hmacSha256;

  ReceiveIntEvent intEventListener;
  ReceiveBooleanEvent booleanEventListener;
  ReceiveStringEvent stringEventListener;
  ReceiveSystemMessageEvent systemMessageEventListener;

  cppQueue *receiverQueue;
  cppQueue *messageQueue;
};

#endif
