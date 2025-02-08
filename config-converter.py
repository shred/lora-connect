#!/usr/bin/env python3
#
# LoRa-Connect
#
# Copyright (C) 2023 Richard "Shred" Körber
#   https://codeberg.org/shred/lora-connect
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#

from base64 import urlsafe_b64encode
import json
import os
import sys

standardErrorMap = {0: 'Off', 1: 'Present', 2: 'Confirmed'}

def main(argv):
    loraKey = bytearray(os.urandom(32))
    loraKeyBase64 = urlsafe_b64encode(loraKey).decode('ASCII').rstrip('=')

    with open(argv[0], "r") as f:
        devices = json.load(f)

    if len(devices) > 1:
        print('Only one appliance is supported at the moment', file=sys.stderr)
        return

    device = devices[0]
    if 'iv' not in device:
        print('Only appliances using port 80 are supported at the moment', file=sys.stderr)
        return

    print('Use these lines in your sender/config.h file:', file=sys.stderr)
    print('', file=sys.stderr)
    print('#define HC_APPLIANCE_KEY "%s"' % (device['key']), file=sys.stderr)
    print('#define HC_APPLIANCE_IV "%s"' % (device['iv']), file=sys.stderr)
    print('', file=sys.stderr)

    print('New random key for your sender/config.h and receiver/config.h file:', file=sys.stderr)
    print('', file=sys.stderr)
    print('#define LORA_ENCRYPT_KEY "%s"' % loraKeyBase64, file=sys.stderr)
    print('', file=sys.stderr)

    featureMap = {}
    valueMap = {}
    for key, description in device['features'].items():
        intKey = int(key)
        featureMap[intKey] = description['name']
        if 'values' in description:
            kvMap = {}
            for vk, vd in description['values'].items():
                kvMap[int(vk)] = vd
            valueMap[intKey] = kvMap

    print('/* THIS FILE WAS AUTO-GENERATED WITH config-converter.py */')
    print('/* All manual changes will be lost. */')
    print()
    print('#include "mapping.h"')
    print()
    print('String mapKey(uint16_t key) {')
    print('  switch (key) {')
    for key, value in sorted(featureMap.items()):
        print('    case %d: return F("%s");' % (key, value))
    print('    default: return String(key, DEC);')
    print('  }')
    print('}')
    print()
    print('String mapIntValue(uint16_t key, int32_t value) {')
    print('  switch (key) {')
    for key, value in sorted(valueMap.items()):
        if value == standardErrorMap:
            print('    case %d:' % (key))

    print('      switch (value) {')
    for vk, vd in standardErrorMap.items():
        print('        case %d: return F("%s");' % (vk, vd))
    print('      }')
    print('      break;')

    for key, value in sorted(valueMap.items()):
        if value != standardErrorMap:
            print('    case %d:' % (key))
            print('      switch (value) {')
            for vk, vd in sorted(value.items()):
                print('        case %d: return F("%s");' % (vk, vd))
            print('      }')
            print('      break;')
    print('  }')
    print('  return F("");');
    print('}')
    print()

if __name__ == "__main__":
   main(sys.argv[1:])
