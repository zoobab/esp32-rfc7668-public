# RFC7668 with BTStack

This project is an example on how to use BTStack as Bluetooth stack together with the ESP-IDF and BLE to have transparent 6lowpan networking with
lwIP.

Please place the files rfc7668.c and rfc7668.h/rfc7668_opts.h in the corresponding lwip folders (components/lwip/include/lwip/netif and components/lwip/netif).

In addition, you need to adjust the path to the BTStack sources (components/btstack/component.mk). ATTENTION: this path is relative!

# Getting Started

* Download/clone this repository
* Download/clone BTStack repo (master branch), use a path nearby this repository (relative path in the Makefile)
* Move these files to the esp-idf lwip components folder: rfc7668.c (components/lwip/netif) and rfc7668.h/rfc7668_opts.h (components/lwip/include/lwip/netif)
* make menuconfig, make and make flash monitor. Please deactivate Component Config->Bluetooth->Bluedroid Bluetooth stack enabled in make menuconfig!
* Connect to your ESP32 via following shell commands (linux only; as root!):
```
modprobe bluetooth_6lowpan
echo 1 > /sys/kernel/debug/bluetooth/6lowpan_enable
hciconfig hci1 reset
echo "connect 24:0A:C4:04:xx:yy 1" > /sys/kernel/debug/bluetooth/6lowpan_control 
ping6 -I bt0 fe80::240a:c4ff:fe04:xxyy
```
Please replace "xx" and "yy" with the last 2 bytes of the BLE MAC address of your ESP32! Afterwards, your ESP32 is already responding to pings!


# Problems?

Either we discuss anything public in the esp-idf repo, maybe this software will be merged to the esp-idf master, or you open an issue here.
