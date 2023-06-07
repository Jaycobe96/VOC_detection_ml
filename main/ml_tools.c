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
#include <math.h>

#define SQUARE_ROOT_PREC (0.00000001f)

static ml_data_type ml_abs(ml_data_type x);
static ml_data_type ml_sgn(ml_data_type x);
static ml_data_type ml_pow(ml_data_type x, uint32_t y);
//static ml_data_type ml_eigenvalue(matrix_data A_data, vector_data v_data);
static ml_data_type ml_vector_norm(vector_data v_data);
static void ml_power_iteration(matrix_data A_ldim_data, ml_data_type *ev_out, vector_data *v_out);

static ml_data_type ml_abs(ml_data_type x) {
	return (x >= 0.0) ? x : -x;
}

static ml_data_type ml_sqrt(ml_data_type x) {
	// initial guess
	x = ml_abs(x);
	ml_data_type x_impr = x/2.0f;
	size_t ite = 0;
	while(x_impr*x_impr - x > SQUARE_ROOT_PREC && ite < 10000) {
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

ml_data_type ml_eigenvalue(matrix_data A_data, vector_data v_data) {
	matrix_data vT_data = vector_transpose(v_data);
	matrix_data prod_data = matrix_mult(A_data, vT_data);
	matrix_t prod = prod_data.data;
	vector_t v = v_data.data;
	ml_data_type ret = prod[0][0] / v[0];
	matrix_free(vT_data);
	matrix_free(prod_data);
	return ret;
}

static ml_data_type ml_vector_norm(vector_data v_data) {
	ml_data_type sum = 0;
	vector_t v = v_data.data;
	for(size_t i = 0; i < v_data.n_len; i++) {
	 	sum += v[i]*v[i];
	}
	return sqrt(sum);
}

static void ml_power_iteration(matrix_data A_ldim_data, ml_data_type *ev_out, vector_data *v_out) {
	const size_t max_ite = 10000;
	size_t ite = 0;
	vector_data v_data = vector_data_mem_init(A_ldim_data.m_len, 0);
	vector_t v = v_data.data;

	ml_data_type m_sqrt = sqrt(v_data.n_len);
	for(size_t i = 0; i < v_data.n_len; i++) {
		v[i] = 1.0f / m_sqrt;
	}

	ml_data_type ev = ml_eigenvalue(A_ldim_data, v_data);
	printf("sta ev: %.3f\n", ev);
	vector_print(v_data, "v_start");
	while(ite < max_ite) {

		// Av = A * v
		matrix_data vT_data = vector_transpose(v_data);
		matrix_data Av_data = matrix_mult(A_ldim_data, vT_data);
		matrix_free(vT_data);

		// Av / ||Av||2
		matrix_data AvT_data = matrix_transpose(Av_data, 1);
		vector_data AvT_vec_data = matrix_data_to_vector_data(AvT_data, 0, 1);
		matrix_free(Av_data);
		ml_data_type norm = ml_vector_norm(AvT_vec_data);
		vector_t AvT_vec = AvT_vec_data.data;
		for(size_t i = 0; i < AvT_vec_data.n_len; i++) {
			AvT_vec[i] /= norm;
		}
		ml_data_type ev_new = ml_eigenvalue(A_ldim_data, AvT_vec_data);
		vector_copy(v_data, AvT_vec_data);

		matrix_free(AvT_data);
		vector_free(AvT_vec_data);

		if(abs(ev - ev_new) < 0.01) {
			ev = ev_new;
			break;
		}
		ev = ev_new;
		printf("ev: %.3f\n", ev);
		vector_print(v_data, "v");
		ite++;
	}
	if(max_ite == ite) {
		printf("ml_svd: power iteration reached max iteration count\n");
	}
	printf("power_iteration: iterations done: %u\n", ite);
	*v_out = v_data;
	if(ev_out != NULL) {
		*ev_out = ev;
	}
}

matrix_data ml_matrix_normalize(matrix_data A_data) {

	matrix_t A = A_data.data;
	for(size_t i = 0; i < A_data.n_len; i++) {
		ml_data_type sd = 0.0, mean = 0.0, sum = 0.0;

		for(size_t j = 0; j < A_data.m_len; j++) {
			sum += A[j][i];
		}
		mean = sum / A_data.m_len;

		sum = 0.0;
		for(size_t j = 0; j < A_data.m_len; j++) {
			sum += (A[j][i] - mean)*(A[j][i] - mean);
		}
		sd = sqrt(sum / A_data.m_len);

		for(size_t j = 0; j < A_data.m_len; j++) {
			A[j][i] = (A[j][i] - mean) / sd;
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
		for(size_t u = i; u < Q_t_data.m_len; u++) {
			R[i][u] += vector_mult(vector_to_vector_data((vector_t)Q_t[i], Q_t_data.n_len),
					vector_to_vector_data(A_t[u], A_t_data.n_len));
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
	matrix_t A_t = A_t_data.data;

	//gram-schmidt process (row-wise)

	// first iteration
	memcpy(U[0], A_t[0], U_data.n_len * sizeof(ml_data_type));

	// iterations
	for(size_t i = 1; i < U_data.m_len; i++) {

		for(size_t j = 1; j <= i; j++) {
			vector_data u_data; u_data.n_len = U_data.n_len; u_data.data = U_data.data[j-1];
			vector_data a_data; a_data.n_len = A_t_data.n_len; a_data.data = A_t[i];
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
			U[i][u] += A_t[i][u];
		}
	}

	//euclidean norm
	for(size_t i = 0; i < U_data.m_len; i++) {
		ml_data_type sum = 0;
		for(size_t u = 0; u < U_data.n_len; u++) {
			sum += U[i][u]*U[i][u];
		}

		ml_data_type eucl = sqrt(sum);
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
					v_sum += pow(A[j][k], 2);
		}
		ml_data_type alpha = 0.0f - ml_sgn(A[k + 1][k])*sqrt(v_sum);
		ml_data_type r = sqrt(0.5f * (pow(alpha, 2) - A[k + 1][k]*alpha));

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

//https://towardsdatascience.com/simple-svd-algorithms-13291ad2eef2
void ml_svd(matrix_data A_data) {
	const ml_data_type k = A_data.n_len;
	matrix_data A_ldim_data = matrix_data_mem_copy(A_data);
	ml_data_type sigma = 0.0f;
	matrix_data U_data = matrix_data_mem_init(A_data.m_len, A_data.m_len, 0);
	matrix_data V_data =  matrix_data_mem_init(A_data.n_len, A_data.n_len, 0);
	vector_data D_data = vector_data_mem_init(A_data.m_len, 0);
	for(size_t i = 0; i < k; i++) {
		if(i > 0) {
			vector_data v_data = matrix_data_to_vector_data(V_data, i - 1, 1);
			vector_data u_data = matrix_data_to_vector_data(U_data, i - 1, 1);
			matrix_data u_mx_data = vector_data_to_matrix_data(u_data, 1);
			matrix_data v_T_data = vector_transpose(v_data);
			vector_t D = D_data.data;
			matrix_data prod_data = matrix_mult(v_T_data, u_mx_data);
			vector_free(v_data);
			vector_free(u_data);
			matrix_free(u_mx_data);
			matrix_free(v_T_data);
			for(size_t u = 0; u < prod_data.m_len; u++) {
				for(size_t i = 0; i < prod_data.n_len; i++) {
					prod_data.data[u][i] *= D[i - 1];
				}
			}
			matrix_subtract(A_ldim_data, prod_data);
			matrix_free(prod_data);
		}
		vector_data u_eigenvector_data;
		ml_power_iteration(A_ldim_data, NULL, &u_eigenvector_data);
		vector_print(u_eigenvector_data, "eigenvector");
		matrix_data A_ldim_T_data = matrix_transpose(A_ldim_data, 1);
		matrix_data u_eigenvector_T_data = vector_transpose(u_eigenvector_data);
		matrix_data v_eigenvector_data = matrix_mult(A_ldim_T_data, u_eigenvector_T_data);
		matrix_data v_eigenvector_T_data = matrix_transpose(v_eigenvector_data, 1);
		vector_data v_eigenvector_T_vec_data = matrix_data_to_vector_data(v_eigenvector_T_data, 0, 1);
		vector_t v = v_eigenvector_T_vec_data.data;
		sigma = ml_vector_norm(v_eigenvector_T_vec_data);
		for(size_t i = 0; i < v_eigenvector_T_vec_data.n_len; i++) {
			v[i] /= sigma;
		}
		vector_data u_ptr = matrix_data_to_vector_data(U_data, i, 0);
		vector_data v_ptr = matrix_data_to_vector_data(V_data, i, 0);
		vector_copy(u_ptr, u_eigenvector_data);
		vector_copy(v_ptr, v_eigenvector_T_vec_data);
		D_data.data[i] = sigma;
		vector_free(u_eigenvector_data);
		matrix_free(A_ldim_T_data);
		matrix_free(u_eigenvector_T_data);
		matrix_free(v_eigenvector_data);
		matrix_free(v_eigenvector_T_data);
		vector_free(v_eigenvector_T_vec_data);
	}
	matrix_print(U_data, "U");
	matrix_print(V_data, "V");
	vector_print(D_data, "D");
}
