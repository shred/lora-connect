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

#ifndef __LoRaSender__
#define __LoRaSender__

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

typedef struct acknowledge {
  uint8_t hash[4];  // MUST be the first element!
  uint16_t number;
  uint8_t pad[MAX_ACK_SIZE - sizeof(hash) - sizeof(number)];
} Acknowledge;
static_assert(sizeof(struct acknowledge) == MAX_ACK_SIZE, "acknowledge structure does not have expected size");

/**
 * LoRa Sender
 */
class LoRaSender {
public:
  /**
   * Constuctor.
   */
  LoRaSender(const char *base64key);

  /**
   * Destructor.
   */
  ~LoRaSender();

  /**
   * Start LoRa connection after everything is set up.
   */
  void connect();

  /**
   * Send an integer value.
   */
  void sendInt(uint16_t key, int32_t value);

  /**
   * Send a boolean value.
   */
  void sendBoolean(uint16_t key, bool value);

  /**
   * Send a string.
   */
  void sendString(uint16_t key, String value);

  /**
   * Send a system message.
   */
  void sendSystemMessage(String message);

  /**
   * Flush buffer, make sure all messages are sent.
   */
  void flush();

  /**
   * Go to sleep, there are no messages expected to be sent.
   */
  void sleep();

  /**
   * Invoked in main loop.
   */
  void loop();


private:
  void sendMessage(uint8_t type, uint16_t key, uint8_t *msg, size_t length);
  void sendRaw(Payload &payload);
  void onLoRaReceive(int packetSize);
  void encryptPayload(Payload &sendPayload);
  void transmitPayload();
  boolean checkAcknowledge(uint8_t *ackPackage);

  Payload payloadBuffer;

  cppQueue *senderQueue;
  cppQueue *acknowledgeQueue;

  bool validEncrypted;
  uint8_t currentEncrypted[sizeof(Payload)];
  size_t currentEncryptedLength;
  uint16_t currentPayloadNumber;
  unsigned long lastSendTime;
  unsigned long nextSendDelay;
  uint8_t attempts;

  uint8_t enckey[SHA256::HASH_SIZE];
  uint8_t mackey[SHA256::HASH_SIZE];
  AES256 aesEncrypt;
  AES256 aesDecrypt;
  SHA256 hmacSha256;
};

#endif
