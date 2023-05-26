/*
 * ml_vector.h
 *
 *  Created on: May 22, 2023
 *      Author: Jakub Tomczak
 */

#ifndef MAIN_ML_VECTOR_H_
#define MAIN_ML_VECTOR_H_

#include <stdint.h>
#include <stdlib.h>
#include "ml_types.h"


vector_data vector_to_vector_data(vector_t v, size_t n_len);
void vector_print(vector_data v_data, const char* v_name);
void vector_free(vector_data v_data);
vector_t vector_mem_copy(vector_data v_data);
vector_data vector_data_mem_copy(vector_data v_data);
vector_t vector_mem_init(size_t n_len, int32_t val_init);
vector_data vector_data_mem_init(size_t n_len, int32_t val_init);
vector_data matrix_data_to_vector_data(matrix_data A_data, size_t at, uint8_t copy);
vector_data vector_copy(vector_data v1_data, vector_data v2_data);
matrix_data vector_transpose(vector_data v_data);
ml_data_type vector_mult(vector_data v1_data, vector_data v2_data);
vector_data vector_mult_scalar(ml_data_type scalar, vector_data v_data, uint8_t copy);


#endif /* MAIN_ML_VECTOR_H_ */
