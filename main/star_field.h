#ifndef STAR_FIELD_H
#define STAR_FIELD_H

#include <stdint.h>

typedef struct {
    uint8_t x;
    uint8_t y;
    uint8_t size;
    uint8_t brightness;
} star_t;

void star_field_init(void);
void star_field_draw(uint8_t brightness, float breath_phase);

#endif // STAR_FIELD_H
