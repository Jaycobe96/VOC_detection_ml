/*
 * matrix.h
 *
 *  Created on: May 20, 2023
 *      Author: Jakub Tomczak
 */

#ifndef MAIN_MATRIX_H_
#define MAIN_MATRIX_H_

#include <stdint.h>
#include <stdlib.h>
#include "ml_types.h"


matrix_data matrix_to_matrix_data(matrix_t A, size_t n_len, size_t m_len);
void matrix_print(matrix_data A_data, const char* A_name);
void matrix_free(matrix_data A_data);
matrix_t matrix_mem_copy(matrix_data A_data);
matrix_data matrix_data_mem_copy(matrix_data A_data);
matrix_data matrix_data_m_concatenate(matrix_data A_data, matrix_data B_data);
matrix_t matrix_mem_init(size_t n_len, size_t m_len, int32_t val_init);
matrix_data matrix_data_mem_init(size_t n_len, size_t m_len, int32_t val_init);
matrix_data vector_data_to_matrix_data(vector_data v, uint8_t copy);
matrix_data matrix_copy(matrix_data A_data, matrix_data B_data);
matrix_data matrix_transpose(matrix_data A_data, uint8_t copy);
matrix_data matrix_subtract(matrix_data A_data, matrix_data B_data);
matrix_data matrix_mult(matrix_data A_data, matrix_data B_data);
matrix_data matrix_mult_scalar(ml_data_type scalar, matrix_data A_data, uint8_t copy);


#endif /* MAIN_MATRIX_H_ */
