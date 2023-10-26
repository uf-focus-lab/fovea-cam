#include "fcmp.h"

uint8_t __fcmp_compose_frame__(fcmp_frame_t *dst, uint8_t header, void *field,
                               uint8_t field_size) {
  dst->checksum = header;
  dst->header = header;
  for (uint8_t i = 0; i < field_size; i++) {
    dst->checksum ^= ((uint8_t *)field)[i];
    dst->field[i] = ((uint8_t *)field)[i];
  }
  return sizeof(fcmp_frame_t) + field_size;
}
