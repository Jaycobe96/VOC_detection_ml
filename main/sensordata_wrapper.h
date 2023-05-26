/*
 * sensordata_wrapper.h
 *
 *  Created on: Jun 15, 2022
 *      Author: Jakub Tomczak
 */

#ifndef MAIN_SENSORDATA_WRAPPER_H_
#define MAIN_SENSORDATA_WRAPPER_H_

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#define BUFF_DATA_COUNT				16
#define GATTS_CHAR_DATA_BUFFER_SIZE 	(BUFF_DATA_COUNT * 4)
#define UINT32_TO_UINT8(target, byte_index) (target >> (byte_index * 8))

typedef struct {
int32_t t_x100;
int32_t p_x100;
int32_t h_x100;
int32_t as;
int32_t g_res;
int32_t mic_int;
int32_t gyro_x;
int32_t gyro_y;
int32_t gyro_z;
}sensors_data_t;

typedef enum {
	BL_PAGE_SENSDAT1 = 0,
	BL_PAGE_SENSDAT2,
	BL_PAGE_XYZ
}bl_page;

void sensordata_set_class(uint32_t new_class);
void sensordata_set_n_class(uint32_t n_class);
uint32_t sensordata_get_class(void);
size_t sensordata_return_sampler_size(void);
void sensordata_set_sampler(uint32_t sampler_N, uint32_t sampler_M, uint32_t interval_sec, uint32_t n_class);
bool sensordata_is_requesting_new_class(void);
bool sensordata_is_sampler_running(void);
void sensordata_new_class_request_clear(void);
void sensordata_resume_sampler(void);
void sensordata_thd_start(void);
void sensordata_split(uint8_t* buff_in, uint8_t* buff_out, uint16_t page, uint16_t page_max);
void sensordata_update(uint32_t* new);
uint32_t* sensordata_get(void);
uint8_t* sensordata_get_buffered_data(void);


#endif /* MAIN_SENSORDATA_WRAPPER_H_ */
