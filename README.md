# LoRa-Connect

Connect your _Home Connect_ device to your home automation via MQTT using LoRa.

## The Key

You need to retrieve your appliance key. This can be done with [hcpy](https://github.com/osresearch/hcpy).

## Hardware

* Heltec Automation LoRa 32 module (2 pcs: one sender, one receiver)

## Dependencies

* WLAN AP: Necessary so the appliance connects to the sender device. Included in ESP32.
* DHCP: Included in ESP32 when used as access point.
* Web Socket Client: [Web Sockets](https://github.com/Links2004/arduinoWebSockets).
* Encryption: HMAC, AES-CBC done via [mbed-tls](https://www.trustedfirmware.org/projects/mbed-tls/).
* LoRa: [Heltec ESP32](https://github.com/HelTecAutomation/Heltec_ESP32), can only be used with the Heltec hardware!
* [ArduinoJson](https://arduinojson.org/)
* [PubSubClient](https://github.com/knolleary/pubsubclient)

## Further Reading

A random collection of (more or less) related links:

* https://trmm.net/homeconnect/
* https://github.com/osresearch/hcpy
* https://www.mischianti.org/2020/12/07/websocket-on-arduino-esp8266-and-esp32-client-1/
* https://techtutorialsx.com/2018/01/25/esp32-arduino-applying-the-hmac-sha-256-mechanism/
* https://mbed-tls.readthedocs.io/en/latest/kb/how-to/encrypt-with-aes-cbc/
* https://www.mischianti.org/2020/12/15/websocket-on-arduino-esp8266-and-esp32-server-authentication-2/

## Kudos

This project would not exist without Trammell Hudson's awesome blog article about ["hacking your dishwasher"](https://trmm.net/homeconnect/) and the related [hcpy](https://github.com/osresearch/hcpy) project. Thank you, Trammell!

