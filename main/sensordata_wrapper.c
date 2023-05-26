/*
 * sensordata_wrapper.c
 *
 *  Created on: Jun 15, 2022
 *      Author: Jakub Tomczak
 */

#include "sensordata_wrapper.h"
#include "pthread.h"
#include "bluetooth_wrapper.h"
#include "spi2_bme688.h"
#include "esp_pthread.h"
#include <string.h>
#include "esp_system.h"
#include "esp_event.h"
#include "sampler.h"
#include "sampler_ctrl.h"
#include "led_ctrl.h"

#define SAMPLING_HZ_DIV 20

bool	 is_sampler_running = false;
bool	 request_new_class = false;
uint32_t sample_at_counter = 1 * SAMPLING_HZ_DIV;

static uint16_t sampler_class_now = 0;
static uint16_t sampler_class_last = 2;

static uint32_t sensors_data[BUFF_DATA_COUNT];
static pthread_t process_sensordata_thread = 0;
static const esp_pthread_cfg_t process_sensordata_cfg = {
		.stack_size = 32768,
		.prio = 2,
		.inherit_cfg = false,
};
void sensordata_set_class(uint32_t new_class) {
		sampler_class_now = new_class;
}

void sensordata_set_n_class(uint32_t n_class) {
	sampler_class_last = n_class;
}

uint32_t sensordata_get_n_class(void) {
	return sampler_class_last;
}

uint32_t sensordata_get_class(void) {
	return sampler_class_now;
}

#define SAMPLER_HEADER_VARS_COUNT 5
static void sensordata_fill_header(uint8_t* header) {
	uint16_t len = sampler_get_sample_count();
	uint8_t n_class = sensordata_get_n_class();
	uint8_t sampler_M = sampler_get_m();
	uint8_t len_bytes[2] = {
			UINT32_TO_UINT8(len, 0),
			UINT32_TO_UINT8(len, 1)};
	uint8_t buff_out[SAMPLER_HEADER_VARS_COUNT] = {len_bytes[0], len_bytes[1], n_class, sampler_M,
			sample_at_counter / SAMPLING_HZ_DIV};
			memcpy(header, buff_out, SAMPLER_HEADER_VARS_COUNT);
			printf("%u, %u, %u, %u, %u", header[0], header[1], header[2], header[3], header[4]);
}

/**
 * return 1 if data dump or sampler not initialized
 */

uint8_t *dumped_data_buffered = NULL;
bool is_dumped_data_avalible = false;
size_t sampler_size = 0;
static uint8_t process_ml_sampling(uint32_t* new_sample) {
	if (!is_sampler_init()) {
		return 1;
	}
	if(new_sample != NULL) {
		sampler_add_sample(new_sample);
		printf("New sample\n");
	}

	if(sampler_is_full()) {
		size_t  header_size = 16;
		sampler_size = sampler_get_size() + header_size;
		//printf("sampler size + header size:%u\n", sampler_size);
		uint8_t* dat_out = malloc(sampler_size);
		assert(dat_out != NULL);
		printf("Dumping data\n");
		sensordata_fill_header(dat_out);
		sampler_dump_raw_data(dat_out + header_size);
		//bt_gatt_send(dat_out, sampler_size);

		if(dumped_data_buffered != NULL ) {
			free(dumped_data_buffered);
		}

		dumped_data_buffered = malloc(sampler_size);
		assert(dumped_data_buffered != NULL);
		memcpy(dumped_data_buffered, dat_out, sampler_size);
		is_dumped_data_avalible = true;

		uint8_t** new_buff = bt_gatt_buffer_init(sampler_size);
		bt_gatt_buffer_set(dat_out, new_buff, sampler_size);
		sampler_ctrl_set_ml_page_buff(new_buff, bt_gatt_size_to_pages_count(sampler_size));
		bt_gatt_send_page(new_buff, 0);

		free(dat_out);
		sampler_deinit();
		return 1;
	}
	return 0;

}

size_t sensordata_return_sampler_size(void) {
	return sampler_size;
}

static bool sensordata_is_res_non_zero(uint32_t* res_data) {
	for(uint16_t i = 0; i < 10; i++) {
		if(res_data[i] == 0) {
			return false;
		}
	}
	return true;
}

// sampling interval in HZ.
void sensordata_set_sampler(uint32_t sampler_N, uint32_t sampler_M, uint32_t interval_sec, uint32_t n_class) {
	sensordata_set_n_class(n_class);
	sample_at_counter = interval_sec * SAMPLING_HZ_DIV;
	sampler_init(sampler_N, sampler_M);
	is_sampler_running = true;
}

bool sensordata_is_requesting_new_class(void) {
	return request_new_class;
}

bool sensordata_is_sampler_running(void) {
	return is_sampler_running;
}

void sensordata_new_class_request_clear(void) {
	request_new_class = 0;
}

void sensordata_resume_sampler(void) {
	is_sampler_running = true;
}

static void *process_sensordata(void* arg) {
	uint32_t sample_now_counter = 0;
	uint32_t sample_class_incr = 0;
	led_ctrl_init(8);
	while(1) {
		uint32_t sensors_data_new[BUFF_DATA_COUNT];
		for(uint16_t i = 0; i < 10; i++) {
			sensors_data_new[i] = bme688_gas_res_now(i);
		}
		sensors_data_new[10] = bme688_celcius_now();
		sensors_data_new[11] = sensordata_get_class();
		sensordata_update(sensors_data_new);

		if(is_sampler_running) {
			//led_ctrl_set_color(0);
			//led_ctrl_steady();
			if(!request_new_class) {
				uint8_t dump = 0;
				if(sample_at_counter <= sample_now_counter && sensordata_is_res_non_zero(sensors_data_new)) {
					dump = process_ml_sampling(sensors_data_new);

					// request new class
					if(!(sampler_get_sample_count() % ((sampler_get_n() / sensordata_get_n_class())))) {
						request_new_class = 1;
					}

					sample_now_counter = 0;
				}
					if(!dump) {
						sample_now_counter++;
					} else {
						is_sampler_running = false;
						request_new_class = false;
						sample_class_incr = 0;
						sensordata_set_class(sample_class_incr);
					}
			} else {
				// on class request increment class and stop sampler
				sensordata_set_class(++sample_class_incr);
				//led_ctrl_set_color(1);
				is_sampler_running = 0;
			}
		}
		bt_indicate_data(sensors_data_new);
		vTaskDelay(10000 / SAMPLING_HZ_DIV / portTICK_PERIOD_MS);
	}
	return NULL;
}

void sensordata_thd_start(void) {
	int res;
	esp_pthread_set_cfg(&process_sensordata_cfg);
	res = pthread_create(&process_sensordata_thread, NULL, process_sensordata, NULL);
	assert(res == 0);
}

void sensordata_split(uint8_t* buff_in, uint8_t* buff_out, uint16_t page, uint16_t page_max) {
	buff_out[0] = UINT32_TO_UINT8(page, 1);
	buff_out[1] = UINT32_TO_UINT8(page, 0);
	buff_out[2] = UINT32_TO_UINT8(page_max, 1);
	buff_out[3] = UINT32_TO_UINT8(page_max, 0);
	uint8_t size_chunk = 16;
	memcpy(buff_out + 4, buff_in + (page * size_chunk), size_chunk);
}

void sensordata_update(uint32_t* new) {
	memcpy(sensors_data, new, sizeof(sensors_data));
}

uint32_t* sensordata_get(void) {
	return sensors_data;
}

uint8_t* sensordata_get_buffered_data(void) {
	if(is_dumped_data_avalible) {
		return dumped_data_buffered;
	} else {
		return NULL;
	}
}


