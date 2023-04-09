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

#ifndef __BASE64URL__
#define __BASE64URL__

#include <Arduino.h>

bool base64UrlDecode(const char *source, uint8_t *target, size_t targetSize);

#endif
