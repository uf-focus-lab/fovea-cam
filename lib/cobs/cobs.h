// =============================================================================
// This is a C implementation of the Consistent Overhead Byte Stuffing (COBS)
// algorithm.
// =============================================================================
// License: MIT
// Author: Yuxuan Zhang (zhangyuxuan@ufl.edu)
// =============================================================================
#ifndef COBS_H
#define COBS_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define COBS_MAX_CONTENT 254
#define COBS_MAX_ENCODED (COBS_MAX_CONTENT + 2)

typedef struct cobs_buffer_s {
  union {
    uint8_t index;
    uint8_t length;
  };
  uint8_t counter;
  // Pseudo element that will be used to store data
  uint8_t data[COBS_MAX_ENCODED];
} cobs_buffer_t;

void cobs_reset(cobs_buffer_t *buf);
/**
 * @brief Encodes a given buffer using COBS algorithm
 *
 * @param src - Buffer of the data to be encoded
 * @param dst - Buffer to store the encoded data
 * @return (int) Indicates whether the encoding is successful.
 *         Positive value indicates output length, excluding tailing zero.
 *         Negative value indicates error.
 */
int cobs_encode(cobs_buffer_t *buf, const uint8_t *input, const uint8_t len);

/**
 * @brief Decodes a given buffer using COBS algorithm.
 *
 * @param src - Buffer of the data to be decoded
 * @param dst - Buffer to store the decoded data
 * @return (int) Indicates whether the encoding is successful.
 *         Positive value indicates number of bytes consumed from the input.
 *                        If positive value is returned, the frame is complete.
 *         Special:   0   indicates frame is not finished.
 *         Negative value indicates decoding error.
 */
int cobs_decode(cobs_buffer_t *buf, const uint8_t *input, const uint8_t len);

/**
 * @brief Utility function to move buffer content n bytes ahead.
 *        The first n bytes will be overwritten.
 * @param head - Pointer to the head of the buffer
 * @param size - Pointer to the size of the buffer (before move)
 * @param offset - Number of bytes to move ahead
 */
static inline void move_ahead(uint8_t *head, unsigned *size, unsigned offset) {
  if (!offset)
    return;
  if (offset >= *size) {
    *size = 0;
  } else {
    *size -= offset;
    if (*size)
      memmove(head, head + offset, *size);
  }
}

#endif
