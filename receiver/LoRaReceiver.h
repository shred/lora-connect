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
#include <SHA256.h>

// Must be a multiple of 16. In the European Union, the maximum permitted LoRa
// payload size is 51, so the next smaller payload size is 48.
#define MAX_PAYLOAD_SIZE 48

// Size of the acknowledge package, must be a multiple of 16.
#define MAX_ACK_SIZE 16

typedef struct payload {
  uint16_t number;
  uint8_t hash[4];
  uint8_t length;
  uint8_t data[MAX_PAYLOAD_SIZE - sizeof(number) - sizeof(hash) - sizeof(length)];
} Payload;
static_assert(sizeof(struct payload) == MAX_PAYLOAD_SIZE, "payload structure does not have expected size");

typedef struct acknowledge {
  uint16_t number;
  uint8_t hash[4];
  uint8_t pad[MAX_ACK_SIZE - sizeof(number) - sizeof(hash)];
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

private:
  void onLoRaReceive(size_t packetSize);
  uint16_t readKey();
  int32_t readInteger(size_t len, bool neg = false);
  String readString();

  Payload payload;
  uint8_t cursor;
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
};

#endif
