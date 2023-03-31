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

#include <Arduino.h>

/**
 * LoRa Sender
 */
class LoRaSender {
public:
  /**
   * Constuctor.
   */
  LoRaSender();

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
   * Invoked in main loop.
   */
  void loop();

private:
  /**
   * Send a raw message.
   */
  void sendMessage(uint8_t type, uint16_t key, uint8_t *msg, size_t length);

  void sendRaw(uint8_t *msg, size_t length);

  uint8_t sendBuffer[100];  // TODO: Find out maximum length of a LoRa package.
  uint8_t sendPosition;

  unsigned long lastSent;
  unsigned long lastFlush;
};

#endif
