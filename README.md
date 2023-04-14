# LoRa-Connect

<img src="img/sender.jpg" width="320px" alt="Sender Module" /> <img src="img/display.jpg" width="320px" alt="Status Display" />

This project connects your _Home Connect_ device to your home automation via MQTT using LoRa.

> **WARNING**: This project is not for the faint-hearted. You **will** need developer skills and at least one hour of your time only to get the configuration straight. You also need to know how to use the Arduino IDE to program the microcontrollers. I won't be able to help you if this project would solve your problem, but your knowledge is not sufficient to set it up.

## Why?

I recently bought a clothes washer that is [Home Connect](https://www.home-connect.com) enabled. Unfortunately, the machine is in a shared laundry room in the basement, and out of reach of my WLAN. I tried to use PLC, but it killed my DSL, so I am unable to connect the machine to the Home Connect cloud.

Thanks to Trammell Hudson's blog article about ["hacking your dishwasher"](https://trmm.net/homeconnect/), I learned that it is possible to directly connect to the machine, without having to use the Home Connect cloud.

This project consists of two ESP32 microcontrollers. One is located at the basement. It connects to the washer, and transmits the current status to my flat using LoRa. The other microcontroller is located in my flat. It receives the status messages from LoRa, converts them into JSON messages, and sends them to MQTT for further processing in my home automation.

Unfortunately (for you), I have no plans to buy more Home Connect appliances in the foreseeable future, so the project will be more or less limited to my washing machine. Also I am only interested in the current status, mainly the remaining time, so it is not possible to remote control the machine. (See also my [To-Do list](TODO.md).)

However I hope that this project is a good starting point for your own experiments and extensions. Please feel free to fork and extend this project, to support more machine types (like dishwashers), or maybe even remote control the machines via MQTT.

## DISCLAIMER

* LoRa uses specific radio frequencies for transmission. The permitted frequencies, maximum transmission power, payload sizes, duty cycles, and other parameters vary from country to country. It is **YOUR responsibility** to properly configure the project, and to ensure it fully complies with the regulations of **YOUR** country. Failure to do so may result in legal problems, claims for damages, and even imprisonment. The author of this project cannot be held liable. This software is designed for use in countries of the European Union. Use in other countries might require changes to the code.
* The project bases on reverse engineering. There is no guarantee that it will work with your Home Connect appliance. It might also stop working some day, e.g. after a firmware update or a protocol change of the manufacturer.
* C isn't the language that I am most proficient with. The source code is ugly, badly formatted, probably has memory leaks, and certainly has a lot of things that a good C developer would do much better. I am open for your constructive feedback. But on the other hand, it is working for me, so I'm fine with it. 😉
* The LoRa transport is hashed and AES256 encrypted, to avoid attackers to read the packages or send fake packages. Also there are resent attempts if a package was lost. However, the encryption is not state-of-the-art, especially because of the very limited LoRa package size. It is also not proof against replay attacks. This is fine for me, but don't expect top notch encryption here.

# Hardware

* 2x Heltec Automation WiFi LoRa 32(V2) modules (one sender, one receiver). With modification to the source, this project might also run on other ESP32 boards with Semtech SX1276/77/78/79 LoRa transceiver.
* A 3D printed case for the module is available at [Printables](https://www.printables.com/model/425740-heltec-lora32-minimal-case).

# Firmware

* The sender firmware can be found in the `sender` directory.
* The receiver firmware can be found in the `receiver` directory.

Both projects need configuration files. To create them, copy the respective `config.h.example` file to `config.h`, and then manually change it to your needs. More about the configuration will follow below.

## Dependencies

You need to install these dependencies in your Arduino library (with the tested version in brackets, but later versions may work too):

* [ArduinoJson](https://arduinojson.org/) by Benoit Blanchon (6.21.0)
* [Base64](https://github.com/Densaugeo/base64_arduino) by Densaugeo (1.3.0)
* [Crypto](https://rweather.github.io/arduinolibs/crypto.html) by Rhys Weatherley (0.4.0)
* [LoRa](https://github.com/sandeepmistry/arduino-LoRa) by Sandeep Mistry (0.8.0)
* [PubSubClient](https://github.com/knolleary/pubsubclient) by Nick O'Leary (2.8.0)
* [Queue](https://github.com/SMFSW/Queue) by SMFSW (1.11)
* [Web Sockets](https://github.com/Links2004/arduinoWebSockets) by Markus Sattler (2.3.6)

# Configuration

Configuring this project is not easy, and will take a considerable amount of time, investigation, and patience. However it is likely to be the most difficult part of the setup.

* First, register your appliance with Home Connect. If there is no WLAN present at your appliance's location, you can also register it using a smartphone and the Home Connect app. The appliance will then spawn an access point for registration.
* After that, use [hcpy hcauth](https://github.com/osresearch/hcpy) to create the `config.json` file.
* In the `sender` and `receiver` directory, you will find `config.h.example` files. Make a copy of each, named `config.h`.
* Now run the `config-converter.py` tool. It will extract the `key` and `iv` values that are required for the next step, and will also generate a `mapping.cpp` file that is needed by the receiver. Invocation is: `./config-converter.py /your/path/to/hcpy/config.json > receiver/mapping.cpp`
* Copy the `HC_APPLIANCE_KEY` and `HC_APPLIANCE_IV` output of the previous step into your `sender/config.h` file. If the `config-converter.py` complains that there is no `iv` value, I'm afraid you're having bad luck. This project only supports websockets via port 80, with a special kind of encryption. If there is no `iv` value, it means that your appliance uses the wss protocol via port 443, with standard SSL. Also, this project currently only supports a single appliance. (See [TODO](TODO.md))
* `config-converter.py` also generates a random encryption key for the LoRa transmission. If you haven't done so yet, copy the `LORA_ENCRYPT_KEY` line into both your `sender/config.h` and `receiver/config.h`. Make sure that both sides are using the same key.
* The `LORA` defines in the `config.h` are depending on your country. To find the correct values, contact the dealer of your LoRa board or check the [frequency plans](https://www.thethingsnetwork.org/docs/lorawan/frequency-plans/). Do not just use values that you have found somewhere on the internet. The `LORA` configuration of the sender and receiver must be identical, otherwise a connection cannot be established.
* The other configuration values depend on your WLAN and MQTT setup. Note that you are actually working with two different WLAN settings. On the _sender_ side, you set up a WLAN AP that your appliance will connect to. On the _receiver_ side, you set the parameters of your existing home WLAN. Both WLANs must have different SSIDs and passwords. (If your appliance is connected to your home WLAN, you actually won't need this solution anyway, but you can just use [hcpy](https://github.com/osresearch/hcpy).)
* Check your `config.h` files again. If they are good, the configuration is finally completed. You can now build and install the sender and receiver firmwares.

# MQTT Format

The MQTT messages are JSON formatted and consist of these keys:

* `key`: The decoded event key. This is a fixed string that is documented by Home Connect. Use this one for event selection.
* `value`: The value of that event. Can be an integer, boolean, or String value depending on the event type.
* `exp`: An expanded, more readable version of `value`, if available. Otherwise this field is missing.
* `uid`: A numerical value of `key`. It is associated with your appliance, and may be different on other appliances. Better use `key`.
* `loraSignalStrength`: RSSI of the LoRa connection to the sender.
* `wifiSignalStrength`: RSSI of your local WLAN.

If a system message is received from the remote sender, the MQTT message consist this key only:

* `systemMessage`: System message string that was received.
* `loraSignalStrength`: RSSI of the LoRa connection to the sender.
* `wifiSignalStrength`: RSSI of your local WLAN.

# Open Source

This project is open source!

## Kudos

* This project would not exist without Trammell Hudson's awesome blog article about ["hacking your dishwasher"](https://trmm.net/homeconnect/) and the related [hcpy](https://github.com/osresearch/hcpy) project. Thank you, Trammell!
* The [CBC implementation](sender/CBC.cpp) was copied from the [arduinolibs](https://github.com/rweather/arduinolibs) crypto legacy by Rhys Weatherley. Note that the license of this project does not apply to the `CBC.cpp` and `CBC.h` files!

## Contribution

* Fork the [Source code at GitHub](https://github.com/shred/lora-connect). Feel free to send pull requests.
* Found a bug? [File a bug report!](https://github.com/shred/lora-connect/issues)

## License

_LoRa-Connect_ is open source software. The source code is distributed under the terms of [GNU General Public License (GPLv3+)](LICENSE.txt).
