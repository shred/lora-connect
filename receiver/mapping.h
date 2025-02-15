/*
 * LoRa-Connect
 *
 * Copyright (C) 2023 Richard "Shred" Körber
 *   https://codeberg.org/shred/lora-connect
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

#ifndef __MAPPING__
#define __MAPPING__

/* NOTE: The mapping.cpp file must be generated by config-converter.py, see README! */

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * Map a key index to the full key name, using the config.json features.
 */
String mapKey(uint16_t key);

/**
 * Map a key value to a String if possible, using the config.json feature values.
 * If there is no string defined for the value, an empty string is returned.
 */
String mapIntValue(uint16_t key, int32_t value);

#endif