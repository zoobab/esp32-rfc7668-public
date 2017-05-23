# RFC7668 with BTStack

This project is an example on how to use BTStack as Bluetooth stack together with the ESP-IDF and BLE to have transparent 6lowpan networking with
lwIP.

Please place the files rfc7668.c and rfc7668.h/rfc7668_opts.h in the corresponding lwip folders (components/lwip/include/lwip/netif and components/lwip/netif).

In addition, you need to adjust the path to the BTStack sources (components/btstack/component.mk). ATTENTION: this path is relative!

# Problems?

Either we discuss anything public in the esp-idf repo, maybe this software will be merged to the esp-idf master, or you open an issue here.
