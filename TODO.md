# To Do

This project is still in a prototype state. It does its job, but there are crucial features still missing. This is a collection of the things that are still open.

## Missing Features

Important missing features that I am going to fix rather sooner than later:

* Sender: LoRa transmission is not secured at all at the moment. Other LoRa receivers can eavesdrop the messages, and also send fake messages. The connection must be encrypted and secured by hashes.
* Sender: Events with too many status chances must be split into separate LoRa transmissions. Currently only the first 8 status changes of an event are sent.

## Known Issues

Known issues that I am going to fix rather sooner than later:

* Sender: `/ro/allDescriptionChanges` and `/ro/allMandatoryValues` actions cannot be requested from the appliance. The response is quite big, and is not received from the websockets library for unknown reasons. This breaks the encryption and causes a reconnection to the appliance. I have found no reason for that behavior yet, and cannot even tell if it is fixable.
* Because of the issue above, the sender is currently unable to send the current status of the appliance, but is only sending changes to the status.

## Wish List

These are nice-to-have features. Maybe I implement them sometimes later, but they have no priority for me.

* This project uses a very simple "fire and forget" LoRa protocol that does not detect if a package was lost in transmission. The receiver should acknowledge each package, and the sender should repeat the package if it was not acknowledged.
* The nice OLED display is totally unused at the moment. It would be great if the display would show the current status of the device when the hardware button is pressed.

## Probably Never Done

These features would certainly make the project useable for more people, but it is very unlikely that I will implement them.

* Support the wss protocol (port 443). Currently this project only supports appliances that communicate via ws (port 80) and a non-standard AES256-CBC encryption. This is certainly a showstopper for some of you, but unfortunately I cannot test the wss protocol because I don't own an appliance for it.
* Support more than one appliance per sender.
* Support other devices than washers.
* Permit to remote-control an appliance via MQTT. Currently only the status of the appliance is transmitted.
