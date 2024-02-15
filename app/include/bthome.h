/*
 * Copyright (c) 2024 Jeffrey C Rizzo
 *
 * SPDX-License-Identifier: MIT
 */

// BTHome device information, from https://bthome.io/format/
#define BTHOME_ENCRYPTION_FLAG 0x01
#define BTHOME_TRIGGER_BASED_FLAG 0x04 // irregular advertising interval
#define BTHOME_VERSION_2 0x40

// object IDs
#define BTHOME_PACKET_ID 0x0
#define BTHOME_BUTTON_EVENT 0x3a

// the event types for the button event
#define BTHOME_BUTTON_EVENT_NONE 0
#define BTHOME_BUTTON_EVENT_PRESS 1
#define BTHOME_BUTTON_EVENT_DOUBLE_PRESS 2
#define BTHOME_BUTTON_EVENT_TRIPLE_PRESS 3
#define BTHOME_BUTTON_EVENT_LONG_PRESS 4
#define BTHOME_BUTTON_EVENT_LONG_DOUBLE_PRESS 5
#define BTHOME_BUTTON_EVENT_LONG_TRIPLE_PRESS 6

#define BTHOME_SERVICE_UUID 0xfcd2
#define BTHOME_SERVICE_UUID_1 0xd2
#define BTHOME_SERVICE_UUID_2 0xfc

#if IS_ENABLED(CONFIG_BTHOME_ENCRYPTION)
#define BTHOME_DEVICE_INFO                                                     \
  (BTHOME_VERSION_2 | BTHOME_TRIGGER_BASED_FLAG | BTHOME_ENCRYPTION_FLAG)
#else
#define BTHOME_DEVICE_INFO (BTHOME_VERSION_2 | BTHOME_TRIGGER_BASED_FLAG)
#endif
