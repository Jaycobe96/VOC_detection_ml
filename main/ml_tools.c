/*
 * ml_tools.c
 *
 *  Created on: May 20, 2023
 *      Author: Jakub Tomczak
 */
#include "ml_tools.h"

#include <string.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_pthread.h"
#include "pthread.h"

#define SQUARE_ROOT_PREC (0.000001f)

static ml_data_type ml_abs(ml_data_type x);
static ml_data_type ml_sgn(ml_data_type x);
static ml_data_type ml_pow(ml_data_type x, uint32_t y);

static ml_data_type ml_abs(ml_data_type x) {
	return (x >= 0.0) ? x : -x;
}

static ml_data_type ml_sqrt(ml_data_type x) {
	// initial guess
	ml_data_type x_impr = x/2.0f;
	size_t ite = 0;
	while(ml_abs(x_impr*x_impr - x) > SQUARE_ROOT_PREC) {
		x_impr = (x_impr + x/x_impr) / 2.0f;
		ite++;
	}
	return x_impr;
}

static ml_data_type ml_sgn(ml_data_type x) {
	if(x > 0.0f) {
		return 1.0f;
	} else if(x == 0.0f) {
		return 1.0f;
	} else if(x < 0.0f) {
		return -1.0f;
	}
	return 1.0f;
}

static ml_data_type ml_pow(ml_data_type x, uint32_t y) {
	if(y == 0.0f) {
		return 1.0f;
	}
	ml_data_type x_out = x;
	for(size_t i = 0; i < y - 1; i++) {
		x_out *= x;
	}
	return x_out;
}

matrix_data ml_matrix_normalize(matrix_data A_data, vector_data *v_diff) {
	matrix_t A = A_data.data;
	printf("%.8f\n",ml_sqrt(2.0));
	if(v_diff != NULL) {
		if(v_diff->n_len != A_data.n_len) {
			printf("ml_matrix_normalize: unequal n lengths");
		}
	}

	for(size_t i = 0; i < A_data.n_len; i++) {
		ml_data_type min=A[0][i], max=0, diff = 0.0;
		for(size_t j = 0; j < A_data.m_len; j++) {
			// find min and max
			if(j == 0) {
				for(size_t c = 0; c < A_data.m_len; c++) {

					if(min > A[c][i]) {
						min = A[c][i];
					}

					if(max < A[c][i]) {
						max = A[c][i];
					}
				}

				diff = max - min;

				if(v_diff != NULL) {
					v_diff->data[i] = diff;
				}
			}
			if(diff != 0.0) {
				A[j][i] = (A[j][i] - min) / diff;
			}
		}
	}

	return A_data;
}

vector_data ml_proj(vector_data u, vector_data a) {
	ml_data_type scalar = vector_mult(u, a) / vector_mult(u, u);
	return vector_mult_scalar(scalar, u, 1);
}


// QR decomposition
// https://en.wikipedia.org/wiki/QR_decomposition
matrix_data ml_calc_R(matrix_data Q_data, matrix_data A_data) {
	matrix_data A_t_data = matrix_transpose(A_data, 1);
	matrix_data Q_t_data = matrix_transpose(Q_data, 1);
	matrix_data R_data = matrix_data_mem_init(A_data.n_len, A_data.n_len, 0);
	matrix_t Q_t = Q_t_data.data;
	matrix_t A_t = A_t_data.data;
	matrix_t R = R_data.data;

	for(size_t i = 0; i < Q_t_data.m_len; i++) {
		for(size_t u = 0; u < Q_t_data.m_len - i; u++) {
			R[i][u] += vector_mult(vector_to_vector_data((vector_t)Q_t[i], A_data.m_len),
					vector_to_vector_data(A_t[u], A_data.m_len));
		}
	}
	return R_data;
}

/**
 * Returns : U matrix
 */
matrix_data ml_calc_Q(matrix_data A_data) {
	matrix_data U_data = matrix_data_mem_init(A_data.m_len, A_data.n_len, 0);
	matrix_t U = U_data.data;
	matrix_data A_t_data = matrix_transpose(A_data, 1);
	matrix_t A = A_t_data.data;

	//gram-schmidt process (row-wise)

	// first iteration
	memcpy(U[0], A[0], U_data.n_len * sizeof(ml_data_type));

	// iterations
	for(size_t i = 1; i < U_data.m_len; i++) {

		for(size_t j = 1; j <= i; j++) {
			vector_data u_data; u_data.n_len = U_data.n_len; u_data.data = U_data.data[j-1];
			vector_data a_data; a_data.n_len = U_data.n_len; a_data.data = A[i];
			vector_data a_proj_data = ml_proj(u_data, a_data);
			vector_t a_proj = a_proj_data.data;

			// subtraction
			for(size_t u = 0; u < U_data.n_len; u++) {
				U[i][u] -= a_proj[u];
			}

			vector_free(a_proj_data);
		}
		// addition
		for(size_t u = 0; u < U_data.n_len; u++) {
			U[i][u] += A[i][u];
		}
	}

	//euclidean norm
	for(size_t i = 0; i < U_data.m_len; i++) {
		ml_data_type sum = 0;
		for(size_t u = 0; u < U_data.n_len; u++) {
			sum += U[i][u]*U[i][u];
		}

		ml_data_type eucl = ml_sqrt(sum);
		// normalize
		for(size_t u = 0; u < U_data.n_len; u++) {
			U[i][u] = U[i][u] / eucl;
		}
	}

	matrix_data ret = matrix_transpose(U_data, 1);
	matrix_free(U_data);
	return ret;
}

matrix_data ml_tridiagonalization(matrix_data A_data) {
	matrix_t A = A_data.data;

	for(size_t k = 0; k < A_data.n_len-2; k++) {
		ml_data_type v_sum = 0;
		for(size_t j = k + 1; j < A_data.n_len; j++) {
					v_sum += ml_pow(A[j][k], 2);
		}
		ml_data_type alpha = 0.0f - ml_sgn(A[k + 1][k])*ml_sqrt(v_sum);
		ml_data_type r = ml_sqrt(0.5f * (ml_pow(alpha, 2) - A[k + 1][k]*alpha));

		vector_data vk_data = vector_data_mem_init(A_data.n_len, 0);
		vector_t vk = vk_data.data;

		vk[k + 1] = (A[k + 1][k] - alpha) / (2.0f*r);
		for(size_t j = k + 2; j < A_data.n_len; j++) {
					vk[j] = A[j][k]/(2.0f*r);
		}

		matrix_data vk_m_data = vector_data_to_matrix_data(vk_data, 1);
		matrix_data vk2_t_data = vector_transpose(vector_mult_scalar(2.0f, vk_data, 1));

		vector_free(vk_data);

		matrix_data vTv_data = matrix_mult(vk2_t_data, vk_m_data);

		matrix_t vTv = vTv_data.data;
		matrix_free(vk2_t_data);
		matrix_free(vk_m_data);

		matrix_data Pk_data = matrix_data_mem_init(A_data.n_len, A_data.n_len, 0);

		// Calculate I-2vTv
		for(size_t u = 0; u < Pk_data.n_len; u++) {
			for(size_t i = 0; i < Pk_data.n_len; i++) {
				if(u == i) {
					Pk_data.data[u][i] = 1.0;
				}
				Pk_data.data[u][i] -= vTv[u][i];
			}
		}

		matrix_data Akp1_data = matrix_mult(Pk_data, A_data);
		matrix_data Akp1_2_data = matrix_mult(Akp1_data, Pk_data);
		matrix_copy(Akp1_2_data, A_data);

		matrix_free(Akp1_data);
		matrix_free(Akp1_2_data);
		matrix_free(Pk_data);
	}

	return A_data;
}
