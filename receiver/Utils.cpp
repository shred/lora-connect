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

/*
 * This is just a wrapper because base64.hpp leads to compilation errors
 * when used in multiple modules of a project.
 */

#include "Utils.h"

#define BASE64_URL  // set base64.hpp to base64url mode
#include <base64.hpp>


void die(const char *message) {
  Serial.print("FATAL: ");
  Serial.println(message);
  while (true)
    ;
}

bool base64UrlDecode(const char *source, uint8_t *target, size_t targetSize) {
  if (decode_base64_length((unsigned char *)source) != targetSize) {
    return false;
  }
  decode_base64((unsigned char *)source, target);
  return true;
}
