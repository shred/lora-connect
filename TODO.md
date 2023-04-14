# To Do

These features would certainly make the project useable for more people, but it is very unlikely that I will implement them in the near future.

* Support of the wss protocol (port 443). Currently this project only supports appliances that communicate via ws (port 80) and a non-standard AES256-CBC encryption. This is certainly a showstopper for some of you, but unfortunately I cannot test the wss protocol because I don't own an appliance for it.
* Other devices than washers should be supported. I don't own one of them though.
* More than one appliance per sender should be supported. This could exceed the permitted duty cycle though.
* The LoRa protocol uses a simple AES256 encryption without a mode of operation. It is acceptable for this purpose, but not state of the art.
* The LoRa protocol isn't immune against replay attacks. The receiver should send a nonce with each acknowledge package, and the sender should use that nonce on the next package.
* It would be great if the appliance could also be remote-controlled via MQTT.
* The nice OLED display is totally unused at the moment. It could show the current status of the device (e.g. connection to appliance, RSSI, number of transmission errors, remaining process time). Unfortunately I found no way to read the PRG button, maybe because of a hardware design error. Keeping the OLED permanently on will wear it down quickly, so it is also not an option.
