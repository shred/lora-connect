# LoRa-Connect

This project connects your _Home Connect_ device to your home automation via MQTT using LoRa.

> WORK IN PROGRESS! This project is currently a prototype that is not working at all yet.

## Why?

I recently bought a clothes washer that is [Home Connect](https://www.home-connect.com) enabled. Unfortunately, the machine is in a shared laundry room in the basement, and out of reach from my WLAN. I tried to use PLC, but it killed my DSL, so I am unable to connect the machine to the Home Connect cloud.

Thanks to Trammell Hudson blog article about ["hacking your dishwasher"](https://trmm.net/homeconnect/), I learned that it is possible to directly connect to the machine, without having to connect to the cloud.

With this project, I now try to connect to the machine using an ESP32 microcontroller that is located in the basement. It reads the current state from the machine, and transmits it to my flat using LoRa. In my flat, another microcontroller receives the status, converts it into JSON messages, and sends them to MQTT for further processing in my home automation.

Unfortunately (for you), I have no plans to buy more Home Connect appliances in the foreseeable future, so the project will be more or less limited to my washing machine. Also I am only interested in the current state, so the communication is unidirectional, and it is not possible to remote control the machine as well.

However I hope that this project is a good starting point for your own experiments and extensions. Please feel free to fork and extend this project, to support more machine types (like dishwashers), or maybe even remote control the machines via MQTT.

## DISCLAIMER

* The project bases on reverse engineering. There is no guarantee that it will work with your Home Connect appliance. It might also stop working some day, e.g. after a protocol change of the manufacturer.
* LoRa uses certain radio frequencies for transmission. The permitted frequencies, maximum transmission power, package sizes, duty cycles, and other parameters differ from country to country. It is **YOUR responsibility** to properly configure the project to comply to the rules of **YOUR** country. Failing to do so may result in legal problems, high damage claims, and even prison. The author of this project cannot be held liable for damages caused by misconfiguration.
* C isn't the language that I am most proficient with. The source code is ugly, badly formatted, and certainly with a lot of things that a good C developer would do much better. I am open for your constructive feedback. But on the other hand, it works for me, so I'm fine with it.

# Hardware

* Heltec Automation LoRa 32 module (2 pcs: one sender, one receiver)
* I designed a 3D printed case for the module that is available at [Printables](https://www.printables.com/model/425740-heltec-lora32-minimal-case).

# Firmware

## Dependencies

* [ArduinoJson](https://arduinojson.org/) by Benoit Blanchon (v6.21.0 or higher)
* [Base64](https://github.com/Densaugeo/base64_arduino) by Densaugeo (v1.3.0 or higher)
* [Crypto](https://rweather.github.io/arduinolibs/crypto.html) by Rhys Weatherley
* [Heltec ESP32 Dev-Boards](https://github.com/HelTecAutomation/Heltec_ESP32) by Heltec Automation (v1.1.1 or higher, can only be used with the Heltec hardware!)
* [PubSubClient](https://github.com/knolleary/pubsubclient) by Nick O'Leary (v2.8.0 or higher)
* [Web Sockets](https://github.com/Links2004/arduinoWebSockets) by Markus Sattler (v2.3.6 or higher)

## Further Reading

A random collection of (more or less) related links:

* https://trmm.net/homeconnect/
* https://github.com/osresearch/hcpy
* https://www.mischianti.org/2020/12/07/websocket-on-arduino-esp8266-and-esp32-client-1/
* https://techtutorialsx.com/2018/01/25/esp32-arduino-applying-the-hmac-sha-256-mechanism/
* https://mbed-tls.readthedocs.io/en/latest/kb/how-to/encrypt-with-aes-cbc/
* https://www.mischianti.org/2020/12/15/websocket-on-arduino-esp8266-and-esp32-server-authentication-2/
* https://github.com/alexantoniades/encrypted-lora-gateway

# Configuration

You need to retrieve your appliance key. This can be done with [hcpy](https://github.com/osresearch/hcpy).

# Open Source

## Kudos

* This project would not exist without Trammell Hudson's awesome blog article about ["hacking your dishwasher"](https://trmm.net/homeconnect/) and the related [hcpy](https://github.com/osresearch/hcpy) project. Thank you, Trammell!
* The CBC implementation was copied from the [arduinolibs](https://github.com/rweather/arduinolibs) crypto legacy by Rhys Weatherley.

## Contribution

* Fork the [Source code at GitHub](https://github.com/shred/lora-connect). Feel free to send pull requests.
* Found a bug? [File a bug report!](https://github.com/shred/lora-connect/issues)

## License

_LoRa-Connect_ is open source software. The source code is distributed under the terms of [GNU General Public License (GPLv3+)](LICENSE.txt).
