#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "hardware/pio.h"

// pinos da matriz de led
#define IS_RGBW false
#define WS2812_PIN 7
#define NUM_PIXELS 25

#ifdef __cplusplus
extern "C" {
#endif

uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b);
void put_pixel(uint32_t pixel_grb);
void set_one_led(uint8_t r, uint8_t g, uint8_t b, bool led_buffer[]);
void atualizarLeds(bool led_buffer[], bool modo);
void matriz_init();

#ifdef __cplusplus
}
#endif
