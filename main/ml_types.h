/*
 * matrix.h
 *
 *  Created on: May 20, 2023
 *      Author: Jakub Tomczak
 */

#ifndef MAIN_ML_TYPES_H_
#define MAIN_ML_TYPES_H_

#include <stdint.h>
#include <stdio.h>

typedef double ml_data_type;
typedef ml_data_type* vector_t;
typedef ml_data_type** matrix_t;

typedef struct{
	size_t n_len;
	size_t m_len;
	matrix_t data;
}matrix_data;

typedef struct{
	ml_data_type *data;
	size_t n_len;
}vector_data;

#endif /* MAIN_MATRIX_H_ */
