#include <stdint.h>
#include <freertos/event_groups.h>

EventGroupHandle_t lowpan_ble_flags;

#define LOWPAN_BLE_FLAG_CONNECTED (1<<0)


#define LOWPAN_BLE_DEBUG_L2CAP_CB 0
