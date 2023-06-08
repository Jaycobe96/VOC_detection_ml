/*
 * matrix.h
 *
 *  Created on: May 20, 2023
 *      Author: Jakub Tomczak
 */

#include "matrix.h"

#include <string.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_pthread.h"
#include "pthread.h"

matrix_data matrix_to_matrix_data(matrix_t A, size_t n_len, size_t m_len) {
	matrix_data out;
	out.data = A;
	out.n_len = n_len;
	out.m_len = m_len;
	return out;
}

void matrix_print(matrix_data A_data, const char* A_name) {
	printf("%s: [\n", A_name);
	for(size_t i = 0; i < A_data.m_len; i++) {
		printf("[");
		for (size_t u = 0; u < A_data.n_len; u++) {
			if(A_data.data[i][u] >= 0.0) {
				printf(" ");
			}
			printf("%.3f", A_data.data[i][u]);
			if(u < A_data.n_len - 1) {
				printf(",\t");
			}
		}
		printf("],\n");
	}
	printf("]\n");
}

void matrix_free(matrix_data A_data) {
	for(size_t i =  A_data.m_len; i > 0; i--) {
		free(A_data.data[i-1]);
	}
	free(A_data.data);
}

matrix_t matrix_mem_copy(matrix_data A_data) {
	const char* err_msg = "Error while allocating matrix\n";
	const size_t col_size = sizeof(ml_data_type*) * A_data.m_len;
	const size_t row_size = sizeof(ml_data_type) * A_data.n_len;
	matrix_t A = (matrix_t) malloc(col_size);

	if(A == NULL) {
		printf(err_msg);
		return NULL;
	}

	for(size_t i = 0; i < A_data.m_len; i++) {
		A[i] = (ml_data_type*) malloc(row_size);
		if(A[i] == NULL) {
			printf(err_msg);
			return NULL;
		}
		memcpy(A[i], A_data.data[i], row_size);
	}
	return A;
}

matrix_data matrix_data_mem_copy(matrix_data A_data) {
	matrix_t A = matrix_mem_copy(A_data);
	return matrix_to_matrix_data(A, A_data.n_len, A_data.m_len);
}

matrix_data matrix_data_m_concatenate(matrix_data A_data, matrix_data B_data) {
	matrix_data C_data = matrix_data_mem_init(A_data.n_len, A_data.m_len + B_data.m_len, 0);
	matrix_t C = C_data.data;
	for(size_t u = 0; u < C_data.m_len; u++) {
		for(size_t i = 0; i < C_data.n_len; i++) {
			if(u < A_data.m_len) {
				C[u][i] = A_data.data[u][i];
			} else {
				C[u][i] = B_data.data[u - A_data.m_len][i];
			}
		}
	}
	return C_data;

}

matrix_t matrix_mem_init(size_t n_len, size_t m_len, int32_t val_init) {
	const char* err_msg = "Error while allocating matrix\n";
	const size_t col_size = sizeof(ml_data_type*) * m_len;
	const size_t row_size = sizeof(ml_data_type) * n_len;
	matrix_t A = (matrix_t) malloc(col_size);

	if(A == NULL) {
		printf(err_msg);
	}

	for(size_t i = 0; i < m_len; i++) {
		A[i] = (ml_data_type*) malloc(row_size);
		if(A[i] == NULL) {
			printf(err_msg);
		}
		memset(A[i], val_init, row_size);
	}
	return A;
}

matrix_data matrix_data_mem_init(size_t n_len, size_t m_len, int32_t val_init) {
	matrix_data A; A.n_len = n_len; A.m_len = m_len;
	A.data = matrix_mem_init(n_len, m_len, val_init);
	return A;
}

matrix_data vector_data_to_matrix_data(vector_data v_data, uint8_t copy) {
	const size_t row_size = sizeof(ml_data_type) * v_data.n_len;
	matrix_data A_data;
	matrix_t A;

	if(copy == 1) {
		A = matrix_mem_init(v_data.n_len, 1, 0);
		memcpy(A[0], v_data.data, row_size);
		A_data.data = A;
	} else {
		A_data.data[0] = v_data.data;
	}

	A_data.n_len = v_data.n_len;
	A_data.m_len = 1;

	return A_data;
}

matrix_data matrix_copy(matrix_data A_data, matrix_data B_data) {
	const size_t row_size = sizeof(ml_data_type) * A_data.n_len;


	if(A_data.n_len != B_data.n_len || A_data.m_len != B_data.m_len) {
		printf("matrix_copy: matrixes must be of same sizes\n");
	}

	for(size_t i = 0; i < A_data.m_len; i++) {
		memcpy(B_data.data[i], A_data.data[i], row_size);
	}
	return B_data;
}

matrix_data matrix_transpose(matrix_data A_data, uint8_t copy) {

	if(A_data.n_len != A_data.m_len) {
		matrix_data new_data;
		matrix_t new = matrix_mem_init(A_data.m_len, A_data.n_len, 0);

		new_data.data = new;
		new_data.n_len = A_data.m_len;

		new_data.m_len = A_data.n_len;

		for(size_t i = 0; i < new_data.m_len; i++) {
			for(size_t u = 0; u < new_data.n_len; u++) {
				new_data.data[i][u] = A_data.data[u][i];
			}
		}
		return new_data;
	}

	if(copy) {
		matrix_t new = matrix_mem_copy(A_data);
		A_data.data = new;
	}

	for(size_t i = 0; i < A_data.m_len; i++) {
		for(size_t u = i; u < A_data.n_len; u++) {
			ml_data_type tmp = A_data.data[i][u];
			A_data.data[i][u] = A_data.data[u][i];
			A_data.data[u][i] = tmp;
		}
	}

	return A_data;
}

matrix_data matrix_subtract(matrix_data A_data, matrix_data B_data) {
	if(A_data.n_len != B_data.n_len || A_data.m_len != B_data.m_len) {
		printf("matrix_mult: A_N must be equal B_M\n");
		return A_data;
	}
	matrix_t A = A_data.data;
	matrix_t B = B_data.data;
	for(size_t u = 0; u < A_data.m_len; u++) {
		for(size_t i = 0; i < A_data.n_len; i++) {
			A[u][i] -= B[u][i];
		}
	}
	return A_data;
}

matrix_data matrix_mult(matrix_data A_data, matrix_data B_data) {
	if(A_data.n_len != B_data.m_len) {
		printf("matrix_mult: A_N must be equal B_M\n");
		return A_data;
	}
	matrix_t A = A_data.data;
	matrix_t B = B_data.data;
	matrix_t C = matrix_mem_init(B_data.n_len, A_data.m_len, 0);

	for(size_t u = 0; u < B_data.n_len; u++) {
		for(size_t i = 0; i < A_data.m_len; i++) {
			ml_data_type sum = 0.0;
			for(size_t j = 0; j < A_data.n_len; j++) {
				sum += A[i][j] * B[j][u];
			}
		C[i][u] = sum;
		}
	}
	matrix_data C_data; C_data.n_len = B_data.n_len; C_data.m_len = A_data.m_len;
	C_data.data = C;
	return C_data;
}

matrix_data matrix_mult_scalar(ml_data_type scalar, matrix_data A_data, uint8_t copy) {
	matrix_t out;

	if(copy == 1) {
		out = matrix_mem_init(A_data.n_len, A_data.m_len, 0);
	} else {
		out = A_data.data;
	}

	for(size_t j = 0; j < A_data.m_len; j++) {
		for(size_t i = 0; i < A_data.n_len; i++) {
			out[j][i] *= scalar;
		}
	}
	A_data.data = out;
	return A_data;
}
