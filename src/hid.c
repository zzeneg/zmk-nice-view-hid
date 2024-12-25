#include <nice_view_hid/hid.h>

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

ZMK_EVENT_IMPL(time_notification);
ZMK_EVENT_IMPL(volume_notification);
ZMK_EVENT_IMPL(layout_notification);
ZMK_EVENT_IMPL(is_connected_notification);

typedef enum {
    _TIME = 0xAA, // random value, must match companion app
    _VOLUME,
    _LAYOUT,
} hid_data_type;

static bool is_connected = false;

static void on_disconnect_timer(struct k_timer *dummy) {
    LOG_INF("raise_connection_notification: false");
    is_connected = false;
    raise_is_connected_notification((struct is_connected_notification){.value = false});
}

K_TIMER_DEFINE(disconnect_timer, on_disconnect_timer, NULL);

static uint8_t last_hid_volume = 0;
static uint8_t last_raised_volume = 0;

static void on_volume_timer(struct k_timer *dummy) {
    // prevent raising event with the same value multiple times
    if (last_raised_volume != last_hid_volume) {
        last_raised_volume = last_hid_volume;
        LOG_INF("raise_volume_notification %i", last_hid_volume);
        raise_volume_notification((struct volume_notification){.value = last_hid_volume});
    }
}

K_TIMER_DEFINE(volume_timer, on_volume_timer, NULL);

void process_raw_hid_data(uint8_t *data, uint8_t length) {
    LOG_INF("display_process_raw_hid_data - received length %u, data_type %u", length, data[0]);

    // raise disconnect notification after 65 seconds of inactivity
    k_timer_start(&disconnect_timer, K_SECONDS(65), K_NO_WAIT);
    if (!is_connected) {
        LOG_INF("raise_connection_notification: true");
        is_connected = true;
        raise_is_connected_notification((struct is_connected_notification){.value = true});
    }

    uint8_t data_type = data[0];
    switch (data_type) {
    case _TIME:
        raise_time_notification((struct time_notification){.hour = data[1], .minute = data[2]});
        break;

    case _VOLUME:
        last_hid_volume = data[1];

        // debounce volume change events
        if (k_timer_status_get(&volume_timer) > 0 || k_timer_remaining_get(&volume_timer) == 0) {
            k_timer_start(&volume_timer, K_MSEC(150), K_NO_WAIT);
            on_volume_timer(&volume_timer);
        }

        break;

    case _LAYOUT:
        raise_layout_notification((struct layout_notification){.value = data[1]});
        break;
    }
}