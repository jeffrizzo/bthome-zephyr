/*
 * Copyright (c) 2024 Jeffrey C Rizzo
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/input/input.h>

#include <zephyr/logging/log.h>

#if IS_ENABLED(CONFIG_SOC_SERIES_ESP32C3)
#include <zephyr/drivers/hwinfo.h>
#endif

#include <bthome.h>
#if IS_ENABLED(CONFIG_BTHOME_ENCRYPTION)
#include <encrypt.h>
#endif
LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

struct bt_le_adv_param adv_param = {
    .id = BT_ID_DEFAULT,
    .sid = 0,
    .secondary_max_skip = 0,
    .options = BT_LE_ADV_OPT_USE_IDENTITY,
    .interval_min = BT_GAP_ADV_FAST_INT_MIN_2,
    .interval_max = BT_GAP_ADV_FAST_INT_MAX_2,
    .peer = NULL,
};

struct bthome_1_byte {
  const uint8_t obj_id; // will get BTHOME_BUTTON_EVENT always!
  uint8_t data;
} __packed;

// packet ID plus 4 buttons
struct bthome_1_byte bthome_data[5] = {
    {BTHOME_PACKET_ID, 0},
    {BTHOME_BUTTON_EVENT, BTHOME_BUTTON_EVENT_NONE},
    {BTHOME_BUTTON_EVENT, BTHOME_BUTTON_EVENT_NONE},
    {BTHOME_BUTTON_EVENT, BTHOME_BUTTON_EVENT_NONE},
    {BTHOME_BUTTON_EVENT, BTHOME_BUTTON_EVENT_NONE},
};
// convenience - will be used as array
struct bthome_1_byte *button = &bthome_data[1];

struct payload {
  uint8_t uuid[2];
  uint8_t device_info;
  struct bthome_1_byte payload[5];
#if IS_ENABLED(CONFIG_BTHOME_ENCRYPTION)
  uint32_t replay_counter;
  uint8_t tag[BTHOME_ENCRYPT_TAG_LEN];
#endif
} __packed;
union {
  struct payload sd;
  uint8_t bytes[sizeof(struct payload)];
} service_data = {
    .sd.uuid = BT_UUID_16_ENCODE(BTHOME_SERVICE_UUID),
    .sd.device_info = BTHOME_DEVICE_INFO,
};

#if IS_ENABLED(CONFIG_BTHOME_ENCRYPTION)
// find a better way to size this
static uint8_t encrypted_buffer[50];
#endif
// convenience pointer to the packet id in service data
uint8_t *packet_id = &(bthome_data[0].data);

// NOTE: the name entry (BT_DATA_NAME_COMPLETE) is optional if we run out of
// space in the advertisement packet. It's also possible to shorten the
// name and use BT_DATA_NAME_SHORTENED instead.  31 bytes (legacy advertisement
// size) is not a lot to work with.  When working with encrypted payload
// (coming soon), we lose an additional 8 bytes, so this is a likely place to
// get it.  The max size of the service data itself is 26 bytes (31 minus
// 3 bytes for the "flags" element, and two bytes for type and data length
// of the service data itself) when leaving the name off.
static struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
#if !IS_ENABLED(CONFIG_BTHOME_ENCRYPTION)
    // when we're encrypting we don't have room for the name
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
            sizeof(CONFIG_BT_DEVICE_NAME) - 1),
#endif
    BT_DATA(BT_DATA_SVC_DATA16, service_data.bytes,
            ARRAY_SIZE(service_data.bytes))};

// set true when we get a button press, clear after sending BLE adv
bool data_ready = false;

static void clear_button_events(void);

// We've got things set up to send key (INPUT_EV_KEY) events, so
// we ignore any other input events. (There shouldn't be any others anyway)
//
// When using the 'longpress' driver, we actually get two different
// key events - one from the gpio (which is also sent to the longpress
// driver), and one from the longpress driver.  We only want the raw
// gpio event if we're NOT using the longpress driver.
// Also, we're only sending the BTHome packet on the press event, not
// the release event.
static void input_cb(struct input_event *evt) {
  LOG_DBG("input type: %u code: %u value: %u", evt->type, evt->code,
          evt->value);

  if (evt->type != INPUT_EV_KEY) {
    LOG_DBG("Ignoring type %u event", evt->type);
    return;
  }

  // ignore press event; we only care about release
  if (evt->value == 1) {
    LOG_DBG("ignoring press event");
    return;
  }

  // Different "keys" for different events; key 0/1/2/3 are the
  // raw buttons (we won't see these because of INPUT_CALLBACK_DEFINE, below),
  // key a/b/c/d are "short press" events, key w/x/y/z are "long press" events
  switch (evt->code) {
  case INPUT_KEY_A:
    LOG_DBG("key A");
    button[0].data = BTHOME_BUTTON_EVENT_PRESS;
    data_ready = true;
    break;
  case INPUT_KEY_B:
    LOG_DBG("key B");
    button[1].data = BTHOME_BUTTON_EVENT_PRESS;
    data_ready = true;
    break;
  case INPUT_KEY_C:
    LOG_DBG("key C");
    button[2].data = BTHOME_BUTTON_EVENT_PRESS;
    data_ready = true;
    break;
  case INPUT_KEY_D:
    LOG_DBG("key D");
    button[3].data = BTHOME_BUTTON_EVENT_PRESS;
    data_ready = true;
    break;
  case INPUT_KEY_W:
    LOG_DBG("key W");
    button[0].data = BTHOME_BUTTON_EVENT_LONG_PRESS;
    data_ready = true;
    break;
  case INPUT_KEY_X:
    LOG_DBG("key X");
    button[1].data = BTHOME_BUTTON_EVENT_LONG_PRESS;
    data_ready = true;
    break;
  case INPUT_KEY_Y:
    LOG_DBG("key Y");
    button[2].data = BTHOME_BUTTON_EVENT_LONG_PRESS;
    data_ready = true;
    break;
  case INPUT_KEY_Z:
    LOG_DBG("key Z");
    button[3].data = BTHOME_BUTTON_EVENT_LONG_PRESS;
    data_ready = true;
    break;
  default:
    LOG_DBG("unknown code/value");
    break;
  };

  // increment packet_id so it's clear this is a new packet
  (*packet_id)++;
}

// we only care about longpress input
INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_NODELABEL(longpress)), input_cb);

static void bt_ready(int err) {
  if (err) {
    LOG_ERR("Bluetooth init failed (err %d)", err);
    return;
  }

  LOG_INF("Bluetooth initialized");
}

static void clear_button_events(void) {
  button[0].data = 0;
  button[1].data = 0;
  button[2].data = 0;
  button[3].data = 0;
}

int main(void) {
  int err;
  bool advertising_started = false;

  // this delay is here to allow the ESP32C3 USB console to be ready
  k_sleep(K_MSEC(500));

  LOG_INF("Starting BTHome Button");
  LOG_INF("Device name is %s", CONFIG_BT_DEVICE_NAME);

#if IS_ENABLED(CONFIG_SOC_SERIES_ESP32C3)
  // workaround for esp32c3 which doesn't (yet) correctly come up with a
  // static address
  bt_addr_le_t ble_addr;

  err = hwinfo_get_device_id(ble_addr.a.val, BT_ADDR_SIZE);
  if (err < 0) {
    LOG_ERR("Could not get device id (err %d)", err);
  } else {
    LOG_HEXDUMP_INF(ble_addr.a.val, BT_ADDR_SIZE, "DEVICE ID");
  }
  ble_addr.type = BT_ADDR_LE_RANDOM;
  BT_ADDR_SET_STATIC(&ble_addr.a);
  err = bt_id_create(&ble_addr, NULL);
  if (err < 0) {
    LOG_ERR("Could not create bt id: err %d", err);
  } else {
    LOG_INF("Created BT id #%d", err);
  }
#endif

  /* Initialize the Bluetooth Subsystem */
  err = bt_enable(bt_ready);
  if (err) {
    LOG_ERR("Bluetooth init failed (err %d)", err);
    return 0;
  }

  size_t count = 1;
  bt_addr_le_t addr = {0};
  char addr_s[BT_ADDR_LE_STR_LEN];

  bt_id_get(&addr, &count);
  bt_addr_le_to_str(&addr, addr_s, sizeof(addr_s));
  LOG_INF("BLE address for advertising: %s", addr_s);
#if IS_ENABLED(CONFIG_BTHOME_ENCRYPTION)
  LOG_INF("initializing crypto module");
  encrypt_init(addr.a.val);
#endif

  for (;;) {
    // there's definitely a better way to do this
    if (data_ready) {
#if IS_ENABLED(CONFIG_BTHOME_ENCRYPTION)
      err = encrypt_ccm(&bthome_data[0].obj_id, sizeof(bthome_data),
                        encrypted_buffer, sizeof(encrypted_buffer),
                        &service_data.sd.tag[0]);
      if (err) {
        LOG_ERR("Encryption failed (err %d)", err);
      } else {
        memcpy(&service_data.sd.payload[0], encrypted_buffer,
               sizeof(bthome_data));
        service_data.sd.replay_counter = replay_counter;
        replay_counter++;
      }
#else
      // copy into service_data before starting to advertise
      memcpy(&service_data.sd.payload[0], &bthome_data[0], sizeof(bthome_data));
#endif
      LOG_DBG("starting adv");
      err = bt_le_adv_start(&adv_param, ad, ARRAY_SIZE(ad), NULL, 0);
      if (err) {
        LOG_ERR("Failed to start advertising (err %d)", err);
      }
      advertising_started = true;
    }
    k_msleep(500);
    if (advertising_started) {
      data_ready = false;
      err = bt_le_adv_stop();
      if (err) {
        LOG_ERR("Advertising failed to stop (err %d)", err);
      }
      advertising_started = false;
      clear_button_events();
    }
  }
  return 0;
}
