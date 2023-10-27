// =============================================================================
// Header File for FoveaCam MEMS Protocol
// THIS IS A LITTLE ENDIAN PROTOCOL
// =============================================================================
// License: MIT
// Author: Yuxuan Zhang (zhangyuxuan@ufl.edu)
// =============================================================================
#ifndef FCMP_H
#define FCMP_H

#include <stdint.h>

enum fcmp_header {
  // 5~8 bit of frame header
  FCMP_METHOD = 0xF0,
  // Frame is meaningless, will be discarded
  FCMP_METHOD_NOP = 0x00,
  // Host side commands
  FCMP_METHOD_SET = 0x10,
  FCMP_METHOD_GET = 0x20, // GET anticipates an ACK with response payload.
  // Device side responses
  FCMP_METHOD_ACK = 0x30,
  FCMP_METHOD_REJ = 0x40,
  // Log messages for debugging purposes
  // The body of the frame will be a variable length string
  FCMP_METHOD_LOG = 0xF0,

  // 0~4 bit of frame header
  FCMP_FIELD = 0x0F,
  // Variable length payload, or empty.
  FCMP_FIELD_ANY = 0x00,
  // Configuration bits
  FCMP_FIELD_CFG = 0x01,
  // Position of MEMS mirror
  FCMP_FIELD_POS = 0x02,
  // MEMS low pass filter clock frequency
  FCMP_FIELD_LPF = 0x03,
  // System reset
  // NOT IMPLEMENTED
  FCMP_FIELD_SYSTEM_RESET = 0x0F,
};

#define TEST_BIT(VALUE, BIT) ((VALUE) & (1 << (BIT)))
#define SET_BIT(VALUE, BIT) ((VALUE) |= (1 << (BIT)))
#define CLR_BIT(VALUE, BIT) ((VALUE) &= ~(1 << (BIT)))
// Configuration bits
enum fcmp_config_bits {
  // Enable MEMS DAC
  FCMP_CFG_BIT_MEMS_EN = 0x0,
  // Toggle strobe synchronization
  FCMP_CFG_BIT_STROBE_SYNC = 0x1,
  // Toggle low-pass filter clock
  FCMP_CFG_BIT_LPF = 0x2,
  // Toggle debug printing mode
  FCMP_CFG_BIT_LOG = 0xF,
};

typedef struct fcmp_frame {
  uint8_t checksum;
  uint8_t header;
  uint8_t field[0];
} fcmp_frame_t;

// Configuration bits
typedef uint16_t fcmp_field_cfg;

// Position getting and setting commands
struct fcmp_field_pos {
  uint16_t ch[4];
};

// MEMS low pass filter clock configuration
// Frequency in Hz, 0 means unchanged (only valid in SET requests).
typedef uint16_t fcmp_field_lpf;

/**
 * @brief Compose any given type and field of frame, with checksum.
 *
 * @param dst (fcmp_frame_t *) FCMP frame to be written into.
 * @param header FCMP_METHOD_XXX | FCMP_FIELD_XXX
 * @param field struct fcmp_field_XXX
 * @param field_size sizeof(struct fcmp_field_XXX)
 */
uint8_t __fcmp_compose_frame__(fcmp_frame_t *dst, uint8_t header, void *field,
                               uint8_t field_size);

#define fcmp_compose_frame(DST, HEADER, FIELD)                                 \
  __fcmp_compose_frame__((fcmp_frame_t *)DST, HEADER, (uint8_t *)&FIELD,       \
                         sizeof(FIELD))

#define fcmp_cobs_tx(COBS_TX_PTR, FCMP_TX_PTR, HEADER, FIELD)                  \
  {                                                                            \
    uint8_t fcmp_size = fcmp_compose_frame(FCMP_TX_PTR, HEADER, FIELD);        \
    cobs_reset(COBS_TX_PTR);                                                   \
    int ret = cobs_encode((COBS_TX_PTR), (uint8_t *)(FCMP_TX_PTR), fcmp_size); \
    if (ret > 0)                                                               \
      Serial.write(cobs_tx.data, (size_t)((COBS_TX_PTR)->length) + 1);         \
    else                                                                       \
      fcmp_printf("Error: COBS encode (%d)", ret);                             \
  }

/**
 * @brief Get the checksum of the given frame.
 *        1. For encoding, set the checksum field to 0,
 *           then set the checksum field to the return value.
 *        2. For decoding, expect the return value to be 0.
 *
 * @param frame The frame to get/check the checksum.
 * @return uint8_t checksum
 */
static inline uint8_t fcmp_checksum(uint8_t *frame, uint8_t size) {
  uint8_t checksum = 0;
  for (uint8_t i = 0; i < size; i++) {
    checksum ^= ((uint8_t *)frame)[i];
  }
  return checksum;
}

#endif
