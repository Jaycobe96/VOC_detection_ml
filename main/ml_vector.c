/*
 * ml_vector.c
 *
 *  Created on: May 22, 2023
 *      Author: Jakub Tomczak
 */
#include "ml_tools.h"

#include <string.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_pthread.h"
#include "pthread.h"

vector_data vector_to_vector_data(vector_t v, size_t n_len) {
	vector_data new; new.data = v; new.n_len = n_len;
	return new;
}

void vector_print(vector_data v_data, const char* v_name) {
	printf("%s: [ ", v_name);
	for(size_t i = 0; i < v_data.n_len; i++) {
		printf("%.3f\t", v_data.data[i]);
	}
	printf("]\n");
}
void vector_free(vector_data v_data) {
	free(v_data.data);
}

vector_t vector_mem_copy(vector_data v_data) {
	const size_t row_size = sizeof(ml_data_type) * v_data.n_len;
	vector_t v_new = (ml_data_type*) malloc(row_size);
	if(v_new == NULL) {
		printf("Error while allocating vector\n");
	}
	memcpy(v_new, v_data.data, row_size);
	return v_new;
}

vector_data vector_data_mem_copy(vector_data v_data) {
	vector_t v = vector_mem_copy(v_data);
	return vector_to_vector_data(v, v_data.n_len);
}

vector_t vector_mem_init(size_t n_len, int32_t val_init) {
	const size_t row_size = sizeof(ml_data_type) * n_len;
	vector_t v_new = (ml_data_type*) malloc(row_size);
	if(v_new == NULL) {
		printf("Error while allocating vector\n");
		return NULL;
	}
	memset(v_new, 0, row_size);
	return v_new;
}

vector_data vector_data_mem_init(size_t n_len, int32_t val_init) {
	vector_data v; v.n_len = n_len; v.data = vector_mem_init(n_len, val_init);
	return v;
}

vector_data matrix_data_to_vector_data(matrix_data A_data, size_t at, uint8_t copy) {
	const size_t row_size = sizeof(ml_data_type) * A_data.n_len;
	vector_data v_data;
	vector_t v;

	if(copy == 1) {
		v = vector_mem_init(A_data.n_len, 0);
		memcpy(v, A_data.data[at], row_size);
		v_data.data = v;
	} else {
		v_data.data = A_data.data[at];
	}

	v_data.n_len = A_data.n_len;

	return v_data;
}

/**
 * copy v2 to v1 if vectors have same lengths
 * returns v1 if vectors have same lengths
 */
vector_data vector_copy(vector_data v1_data, vector_data v2_data) {
	vector_t v_first, v_second;
	size_t n_len;

	if(v2_data.n_len > v1_data.n_len) {
		n_len = v2_data.n_len;
		v_first = v2_data.data;
		v_second = v1_data.data;
	} else {
		n_len = v1_data.n_len;
		v_first = v1_data.data;
		v_second = v2_data.data;
	}

	for(size_t i = 0; i < n_len; i++) {
		v_first[i] = v_second[i];
	}
	vector_data out; out.n_len = n_len; out.data = v_first;
	return out;
}

matrix_data vector_transpose(vector_data v_data) {
	matrix_t A_new = matrix_mem_init(1, v_data.n_len, 0);

	for(size_t i = 0; i < v_data.n_len; i++) {
		A_new[i][0] = v_data.data[i];
	}
	matrix_data A_new_data; A_new_data.data = A_new; A_new_data.n_len = 1;
	A_new_data.m_len = v_data.n_len;
	return A_new_data;
}

ml_data_type vector_mult(vector_data v1_data, vector_data v2_data) {
	if(v1_data.n_len != v2_data.n_len) {
		printf("vector_mult: vectors must be of same lengths");
	}
	ml_data_type sum = 0;
	for(size_t i = 0; i < v1_data.n_len; i++) {
		sum += v1_data.data[i] * v2_data.data[i];
	}
	return sum;
}

vector_data vector_mult_scalar(ml_data_type scalar, vector_data v_data, uint8_t copy) {
	vector_t out = NULL;

	if(copy == 1) {
		out = vector_mem_copy(v_data);
	} else {
		out = v_data.data;
	}

	if(out == NULL) {
		printf("vector_mult_scalar: error: empty vector\n");
	}

	for(size_t i = 0; i < v_data.n_len; i++) {
		out[i] *= scalar;
	}
	v_data.data = out;
	return v_data;
}
