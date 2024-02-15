/*
 * Copyright (c) 2024 Jeffrey C Rizzo
 *
 * SPDX-License-Identifier: MIT
 */

#define BTHOME_ENCRYPT_TAG_LEN 4

extern uint32_t replay_counter;
void encrypt_init(uint8_t ble_addr[6]);
int encrypt_ccm(uint8_t *buf, size_t buflen, uint8_t *, size_t, uint8_t *);