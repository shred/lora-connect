# To Do

This project is still in a prototype state. It does its job, but there are crucial features still missing. This is a collection of the things that are still open.

## Wish List

These are nice-to-have features. Maybe I implement them sometimes later, but they have no priority for me.

* Currently every LoRa package is 48 bytes long, even if the actual payload is much smaller. The packages should be 32 or even 16 bytes long if the payload permits it.
* The LoRa protocol isn't immune against replay attacks. The receiver should send a nonce with each acknowledge package, and the sender should use that nonce on the next package.
* The nice OLED display is totally unused at the moment. It would be great if the display would show the current status of the device when the hardware button is pressed.

## Probably Never Done

These features would certainly make the project useable for more people, but it is very unlikely that I will implement them.

* Support the wss protocol (port 443). Currently this project only supports appliances that communicate via ws (port 80) and a non-standard AES256-CBC encryption. This is certainly a showstopper for some of you, but unfortunately I cannot test the wss protocol because I don't own an appliance for it.
* The LoRa protocol uses a simple AES256 encryption without a mode of operation. It is acceptable for this purpose, but not state of the art.
* Support more than one appliance per sender.
* Support other devices than washers.
* Permit to remote-control an appliance via MQTT. Currently only the status of the appliance is transmitted.
