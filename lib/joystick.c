#include "joystick.h"
#include "hardware/adc.h"
#include "pico/stdlib.h"

static uint16_t center_x = 0;
static uint16_t center_y = 0;

void joystick_init() {
    adc_init();
    adc_gpio_init(JOYSTICK_X_PIN); // GPIO26 = ADC0
    adc_gpio_init(JOYSTICK_Y_PIN); // GPIO27 = ADC1

    center_x = joystick_get_center(0);
    center_y = joystick_get_center(1);
}

uint16_t joystick_get_center(uint8_t adc_channel) {
    uint32_t sum = 0;
    for (int i = 0; i < 100; i++) {
        adc_select_input(adc_channel);
        sum += adc_read();
        sleep_ms(10);
    }
    return sum / 100;
}

void joystick_leitura_bruta(uint16_t *x, uint16_t *y){
    adc_select_input(0); // X
    *x = adc_read();
    adc_select_input(1); // Y
    *y = adc_read();
}

void joystick_leitura_corrigida(int16_t *dx, int16_t *dy){
    uint16_t raw_x, raw_y;
    joystick_leitura_bruta(&raw_x, &raw_y);
    *dx = raw_x - center_x;
    *dy = raw_y - center_y;
}
