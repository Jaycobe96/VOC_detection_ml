/*
 * ml_sampler.h
 *
 *  Created on: May 18, 2023
 *      Author: Jakub Tomczak
 */

#ifndef MAIN_ML_SAMPLER_H_
#define MAIN_ML_SAMPLER_H_

#include <stdint.h>
#include <stdlib.h>
#include "ml_types.h"
#include "matrix.h"

size_t ml_sampler_get_M_len(void);
void ml_sampler_data_clear_all(void);
void ml_sampler_init(size_t M_length, size_t buff_max_size_bytes);
matrix_data ml_sampler_data_collect(void);
uint8_t ml_sampler_data_push(ml_data_type* data);
uint8_t ml_sampler_data_pop(ml_data_type* data);



#endif /* MAIN_ML_SAMPLER_H_ */
