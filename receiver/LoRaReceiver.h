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
  LoRaReceiver();

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

  uint8_t receiveBuffer[128];  // TODO: Find out maximum length of a LoRa package.
  size_t receiveLength;
  uint8_t cursor;

  ReceiveIntEvent intEventListener;
  ReceiveBooleanEvent booleanEventListener;
  ReceiveStringEvent stringEventListener;
  ReceiveSystemMessageEvent systemMessageEventListener;
};

#endif
