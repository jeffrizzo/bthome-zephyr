/*
 * Copyright (c) 2024 Jeffrey C Rizzo
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/input/input.h>

#include <zephyr/logging/log.h>

#include <bthome.h>
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

// we're going to change the button events (buttons 0,1,2,3) directly,
// these are the indices into service_data
#define BUT0_IDX 6
#define BUT1_IDX 8
#define BUT2_IDX 10
#define BUT3_IDX 12
#define PACKETID_IDX 4

static uint8_t service_data[] = {
    BT_UUID_16_ENCODE(BTHOME_SERVICE_UUID),
    BTHOME_VERSION_2 | BTHOME_TRIGGER_BASED_FLAG, /* device information */
    BTHOME_PACKET_ID, /* packet id to weed out duplicates */
    0,
    BTHOME_BUTTON_EVENT, /* Button 0 */
    BTHOME_BUTTON_EVENT_NONE,
    BTHOME_BUTTON_EVENT, /* Button 1 */
    BTHOME_BUTTON_EVENT_NONE,
    BTHOME_BUTTON_EVENT, /* Button 2 */
    BTHOME_BUTTON_EVENT_NONE,
    BTHOME_BUTTON_EVENT, /* Button 3 */
    BTHOME_BUTTON_EVENT_NONE,
};

// pointer to the packet id in service data
uint8_t *packet_id = &service_data[PACKETID_IDX];

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
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
            sizeof(CONFIG_BT_DEVICE_NAME) - 1),
    BT_DATA(BT_DATA_SVC_DATA16, service_data, ARRAY_SIZE(service_data))};

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
    service_data[BUT0_IDX] = BTHOME_BUTTON_EVENT_PRESS;
    data_ready = true;
    break;
  case INPUT_KEY_B:
    LOG_DBG("key B");
    service_data[BUT1_IDX] = BTHOME_BUTTON_EVENT_PRESS;
    data_ready = true;
    break;
  case INPUT_KEY_C:
    LOG_DBG("key C");
    service_data[BUT2_IDX] = BTHOME_BUTTON_EVENT_PRESS;
    data_ready = true;
    break;
  case INPUT_KEY_D:
    LOG_DBG("key D");
    service_data[BUT3_IDX] = BTHOME_BUTTON_EVENT_PRESS;
    data_ready = true;
    break;
  case INPUT_KEY_W:
    LOG_DBG("key W");
    service_data[BUT0_IDX] = BTHOME_BUTTON_EVENT_LONG_PRESS;
    data_ready = true;
    break;
  case INPUT_KEY_X:
    LOG_DBG("key X");
    service_data[BUT1_IDX] = BTHOME_BUTTON_EVENT_LONG_PRESS;
    data_ready = true;
    break;
  case INPUT_KEY_Y:
    LOG_DBG("key Y");
    service_data[BUT2_IDX] = BTHOME_BUTTON_EVENT_LONG_PRESS;
    data_ready = true;
    break;
  case INPUT_KEY_Z:
    LOG_DBG("key Z");
    service_data[BUT3_IDX] = BTHOME_BUTTON_EVENT_LONG_PRESS;
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
  service_data[BUT0_IDX] = 0;
  service_data[BUT1_IDX] = 0;
  service_data[BUT2_IDX] = 0;
  service_data[BUT3_IDX] = 0;
}

int main(void) {
  int err;
  bool advertising_started = false;

  // this delay is here to allow the ESP32C3 USB console to be ready
  k_sleep(K_MSEC(500));

  LOG_INF("Starting BTHome Button");
  LOG_INF("Device name is %s", CONFIG_BT_DEVICE_NAME);

  /* Initialize the Bluetooth Subsystem */
  err = bt_enable(bt_ready);
  if (err) {
    LOG_ERR("Bluetooth init failed (err %d)", err);
    return 0;
  }

  for (;;) {
    // there's probably a better way to do this
    if (data_ready) {
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
