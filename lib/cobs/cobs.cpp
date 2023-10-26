// =============================================================================
// This is a C implementation of the Consistent Overhead Byte Stuffing (COBS)
// algorithm.
// =============================================================================
// License: MIT
// Author: Yuxuan Zhang (zhangyuxuan@ufl.edu)
// =============================================================================
#include "cobs.h"

void cobs_reset(cobs_buffer_t *buf) {
  buf->index = 0;
  buf->counter = 0;
};

int cobs_encode(cobs_buffer_t *buf, const uint8_t *input,
                const uint8_t len) {
  for (uint8_t i = 0; i < len; i++) {
    // Check for buffer overflow
    if (buf->index == COBS_MAX_CONTENT) {
      return -1;
    }
    // Forward Iteration
    buf->index++;
    buf->counter++;
    // Handle next byte
    if (input[i] == 0) {
      // Handle zero byte
      buf->data[buf->index - buf->counter] = buf->counter;
      buf->counter = 0;
    } else {
      // Handle non-zero byte
      buf->data[buf->index] = input[i];
    }
  }
  // Temporary fill the counter byte
  buf->data[buf->index - buf->counter] = buf->counter + 1;
  // Append zero byte
  buf->data[++buf->index] = 0;
  // Return length including tailing zero byte
  return ((int)buf->index);
};

int cobs_decode(cobs_buffer_t *buf, const uint8_t *input,
                const uint8_t len) {
  uint8_t i = 0;
  for (; i < len; i++) {
    // Handle next byte
    const uint8_t next_byte = input[i];
    // Check for reserved zero byte
    if (next_byte == 0) {
      if (buf->counter == 0) {
        // Ignore extra zero bytes
        continue;
      } else if (buf->counter == 1) {
        // End of frame
        return i + 1;
      } else {
        // Error: unexpected zero byte
        return -2;
      }
    }
    // Check for frame preamble
    if (buf->index == 0 && buf->counter == 0) {
      buf->counter = next_byte;
      continue;
    }
    // Check for illegal state: counter should not be zero
    if (buf->counter == 0) {
      // Error: unexpected zero byte
      return -3;
    }
    // Check for buffer overflow
    if (buf->index >= COBS_MAX_CONTENT) {
      return -1;
    }
    // Normal control flow
    if (buf->counter == 1) {
      buf->counter = next_byte;
      // Treat it as a valid zero data byte
      buf->data[buf->index] = 0;
    } else {
      // Handle non-zero byte
      buf->data[buf->index] = next_byte;
      buf->counter--;
    }
    buf->index++;
  }
  if (buf->index == COBS_MAX_CONTENT + 1 && buf->counter == 1) {
    // Max content length (254 bytes) may omit tailing zero byte
    return i;
  } else {
    // Default case: return unfinished flag
    return 0;
  }
}
