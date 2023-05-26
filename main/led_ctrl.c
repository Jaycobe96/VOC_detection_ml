#include "led_ctrl.h"

#include "esp_system.h"
#include "pthread.h"
#include "esp_pthread.h"
#include "esp_event.h"
#include "led_strip.h"
#include "hw.h"

typedef struct{
	bool active;
	bool process_blinking;
	uint32_t red;
	uint32_t green;
	uint32_t blue;
	uint32_t p_intensity;
}led_ctrl_state_t;

static void process_led_ctrl(void* arg);

static led_ctrl_state_t led_ctrl_state = {0};
static led_strip_t* pStrip_a = NULL;

static uint8_t blink_state = 0;
static led_ctrl_color color_now = 99; // undefined

static esp_timer_handle_t led_ctrl_timer = {0};

const esp_timer_create_args_t led_ctrl_timer_args = {
	.callback = &process_led_ctrl,
	.name = "hwtimer_lis3dh"
};

static void led_ctrl_timer_event_start(void) {
	ESP_ERROR_CHECK(esp_timer_create(&led_ctrl_timer_args, &led_ctrl_timer));
	ESP_ERROR_CHECK(esp_timer_start_periodic(led_ctrl_timer, 500000));
}

static void process_led_ctrl(void* arg) {
			if(led_ctrl_state.process_blinking){
				if(blink_state) {
					pStrip_a->clear(pStrip_a, 50);
					pStrip_a->set_pixel(pStrip_a, 0,
							led_ctrl_state.red, led_ctrl_state.green, led_ctrl_state.blue);
					pStrip_a->refresh(pStrip_a, 50);
					blink_state = 0;
				} else {
					pStrip_a->clear(pStrip_a, 50);
					blink_state = 1;
				}

			}
	return;
}

void led_ctrl_init(uint32_t p_intensity) {
	pStrip_a = led_strip_init(1, HW_PWM_LED, 1);
	pStrip_a->clear(pStrip_a, 50);
	led_ctrl_state.p_intensity = p_intensity;
	led_ctrl_state.process_blinking = false;
	led_ctrl_timer_event_start();
}

void led_ctrl_clear(void) {
	pStrip_a->clear(pStrip_a, 50);
}

void led_ctrl_blink(void) {
	led_ctrl_state.process_blinking = true;
}

void led_ctrl_steady(void) {
	led_ctrl_state.process_blinking = false;
}

void led_ctrl_set_intensity(uint32_t p_intensity) {
	if(p_intensity > 100) {
		p_intensity = 100;
	}
	led_ctrl_state.p_intensity = p_intensity;
	pStrip_a->set_pixel(pStrip_a, 0, led_ctrl_state.red, led_ctrl_state.green, led_ctrl_state.blue);
	pStrip_a->refresh(pStrip_a, 50);
}

void led_ctrl_set_color(led_ctrl_color color) {
	if(color_now == color) {
		return;
	}
	switch(color) {
	case (RED) : led_ctrl_set(255, 0, 0);  break;
	case (GREEN) : led_ctrl_set(0, 255, 0); break;
	case (BLUE) : led_ctrl_set(0, 0, 255); break;
	case (YELLOW) : led_ctrl_set(255, 255, 0); break;
	case (PURPLE) : led_ctrl_set(255, 0, 255); break;
	case (LIGHT_BLUE) : led_ctrl_set(0, 255, 255); break;
	case (ORANGE) : led_ctrl_set(255, 128, 0); break;
	default : break;
	}

	color_now = color;
}

void led_ctrl_set(uint32_t red, uint32_t green, uint32_t blue) {
	led_ctrl_clear();
	led_ctrl_state.red = red ? (led_ctrl_state.p_intensity) * red / 100 : 0;
	led_ctrl_state.blue = blue ? (led_ctrl_state.p_intensity) * blue / 100 : 0;
	led_ctrl_state.green = green ? (led_ctrl_state.p_intensity) * blue / 100 : 0;
	pStrip_a->set_pixel(pStrip_a, 0, led_ctrl_state.red, led_ctrl_state.green, led_ctrl_state.blue);
	pStrip_a->refresh(pStrip_a, 50);
}
