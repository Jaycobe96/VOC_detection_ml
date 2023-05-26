/*
 * sampler.h
 *
 *  Created on: 5 Oct 2022
 *      Author: JaycobePC
 */

#ifndef MAIN_SAMPLER_H_
#define MAIN_SAMPLER_H_

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

// default parameters
#define STR_LEN_MAX 10
#define SAMPLER_M 12
#define SAMPLER_N 100

bool is_sampler_init(void);
void sampler_deinit(void);
void sampler_init(uint32_t sampler_N, uint32_t sampler_M);

void sampler_add_sample(uint32_t *sensor_data);
void sampler_clear(void);
bool sampler_is_full(void);
uint32_t sampler_get_sample_count(void);
size_t sampler_get_size(void);
size_t sampler_get_buffered_size(void);
uint32_t sampler_get_n(void);
uint32_t sampler_get_m(void);
void sampler_put_at(uint32_t* sensor_data, uint32_t at_N);
void sampler_dump_data(uint32_t* out);
void sampler_dump_raw_data(uint8_t* out);
void sampler_copy_buff_data(uint8_t* out);

/**
 * SAMPLE FORMAT
 *  M = 12
 * 	M_SIZE = M * int32_t = 48 bytes
 * 	N_SIZE = 100 rows (48 * 100) = ~4.8kB
 * 	ATR_NAMES_SIZE = M * [STR_LEN_MAX]
 */




#endif /* MAIN_SAMPLER_H_ */
