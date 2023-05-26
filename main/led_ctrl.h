#ifndef MAIN_LED_CTRL_H_
#define MAIN_LED_CTRL_H_

#include <stdint.h>
#include <stdbool.h>

typedef enum{
	BLUE = 0,
	RED,
	YELLOW,
	PURPLE,
	GREEN,
	LIGHT_BLUE,
	ORANGE,
}led_ctrl_color;

void led_ctrl_init(uint32_t p_intensity);
void led_ctrl_clear(void);
void led_ctrl_blink(void);
void led_ctrl_steady(void);
void led_ctrl_set_intensity(uint32_t p_intensity);
void led_ctrl_set_color(led_ctrl_color color);
void led_ctrl_set(uint32_t red, uint32_t green, uint32_t blue);

#endif
