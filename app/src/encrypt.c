/*
 * Copyright (c) 2024 Jeffrey C Rizzo
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#if IS_ENABLED(CONFIG_BTHOME_ENCRYPTION)
#include <zephyr/crypto/crypto.h>
#include <zephyr/random/random.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(encrypt, LOG_LEVEL_DBG);

#include <bthome.h>
#include <encrypt.h>
// hardcoderific

/* see https://bthome.io/encryption/ for more info */

// counter for replay protection
uint32_t replay_counter;

#if 0
struct nonce {
  uint8_t ble_mac[6];
  uint16_t uuid;
  uint8_t device_data;
  uint32_t replay_counter;
};
#endif

// key will actually be set in crypto_init()

static uint8_t key[16];
uint8_t nonce[13] = {0};

const struct device *crypto_dev;
void encrypt_init(uint8_t ble_addr[6]) {
  crypto_dev = device_get_binding(CONFIG_CRYPTO_TINYCRYPT_SHIM_DRV_NAME);
  if (!crypto_dev) {
    LOG_ERR("%s pseudo device not found",
            CONFIG_CRYPTO_TINYCRYPT_SHIM_DRV_NAME);
    return;
  }
  // start the replay counter at a random number
  sys_csrand_get(&replay_counter, sizeof(replay_counter));

  // Install ble_addr in the nonce - this part won't change
  // BThome wants the BLE address in the opposite order
  nonce[0] = ble_addr[5];
  nonce[1] = ble_addr[4];
  nonce[2] = ble_addr[3];
  nonce[3] = ble_addr[2];
  nonce[4] = ble_addr[1];
  nonce[5] = ble_addr[0];
  nonce[6] = BTHOME_SERVICE_UUID_1;
  nonce[7] = BTHOME_SERVICE_UUID_2;
  nonce[8] = BTHOME_DEVICE_INFO;

  if (sizeof(CONFIG_BTHOME_ENCRYPTION_KEY) > 1) {
    uint8_t psk[1 + (sizeof(CONFIG_BTHOME_ENCRYPTION_KEY) / 2)];
    size_t len =
        hex2bin(CONFIG_BTHOME_ENCRYPTION_KEY,
                sizeof(CONFIG_BTHOME_ENCRYPTION_KEY) - 1, psk, sizeof(psk));
    if (len <= 0) {
      LOG_ERR("PSK conversion is incorrect! Using default");
    } else if (len != 16) {
      LOG_ERR("PSK wrong size! expected 16, got %u", len);
    } else {
      LOG_HEXDUMP_DBG(psk, 16, "PSK");
      LOG_INF("copying 16 byte PSK: %s", CONFIG_BTHOME_ENCRYPTION_KEY);
      memcpy(key, psk, 16);
    }
  }
}

int encrypt_ccm(uint8_t *plaintext_buf, size_t plaintext_buflen,
                uint8_t *encrypted_buf, size_t encrypted_buflen, uint8_t *tag) {
  int err;

  // Copy the replay counter into the nonce
  memcpy(&nonce[9], &replay_counter, 4);

  struct cipher_ctx context = {
      .keylen = sizeof(key), // sizeof key for this app is always 16
      .key.bit_stream = key,
      .mode_params.ccm_info =
          {
              .nonce_len = sizeof(nonce),
              .tag_len = BTHOME_ENCRYPT_TAG_LEN,
          },
      .flags = CAP_RAW_KEY | CAP_SYNC_OPS | CAP_SEPARATE_IO_BUFS,
  };

  struct cipher_pkt encrypt_pkt = {
      .in_buf = plaintext_buf,
      .in_len = plaintext_buflen,
      .out_buf_max = encrypted_buflen,
      .out_buf = encrypted_buf,
  };

  struct cipher_aead_pkt ccm_op = {
      .ad = NULL,
      .ad_len = 0,
      .pkt = &encrypt_pkt,
      /* TinyCrypt puts the tag at the end of the ciphertext */
      .tag = encrypted_buf + plaintext_buflen,
  };

  err = cipher_begin_session(crypto_dev, &context, CRYPTO_CIPHER_ALGO_AES,
                             CRYPTO_CIPHER_MODE_CCM, CRYPTO_CIPHER_OP_ENCRYPT);
  if (err) {
    LOG_ERR("cipher_begin_session returned %d", err);
    return err; // FAIL
  };

  err = cipher_ccm_op(&context, &ccm_op, nonce);
  if (err) {
    LOG_ERR("Encrypt failed: err %d", err);
    goto out;
  }

  memcpy(tag, encrypted_buf + plaintext_buflen, BTHOME_ENCRYPT_TAG_LEN);
out:
  cipher_free_session(crypto_dev, &context);
  return err;
}
#endif /* CONFIG_BTHOME_ENCRYPTION */