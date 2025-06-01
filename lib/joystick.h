#ifndef JOYSTICK_H
#define JOYSTICK_H

#include "pico/stdlib.h"

#define JOYSTICK_X_PIN 26
#define JOYSTICK_Y_PIN 27
#define JOYSTICK_BTN_PIN 22

void joystick_init();
uint16_t joystick_get_center(uint8_t adc_channel);
void joystick_leitura_bruta(uint16_t *x, uint16_t *y);
void joystick_leitura_corrigida(int16_t *dx, int16_t *dy);

#endif
