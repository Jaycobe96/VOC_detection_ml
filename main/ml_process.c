/*
 * ml_process.c
 *
 *  Created on: May 20, 2023
 *      Author: Jakub Tomczak
 */

#include "ml_process.h"

#include <string.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_pthread.h"
#include "pthread.h"

#include "ml_sampler.h"
#include "i2c2_bme688.h"
#include "matrix.h"
#include "ml_vector.h"
#include "ml_tools.h"
#include "heapsort.h"

static pthread_t ml_process_thread = 0;

static const esp_pthread_cfg_t ml_process_cfg = {
		.stack_size = 131072,
		.prio = 2,
		.inherit_cfg = false,
};

static void *ml_process(void* arg);

void ml_process_thd_start(void) {
	int res;
	esp_pthread_set_cfg(&ml_process_cfg);
	res = pthread_create(&ml_process_thread, NULL, ml_process, NULL);
	assert(res == 0);
}

static void *ml_process(void* arg) {

	const size_t c_samples_trig = 30;

	//
	const size_t c_samples_omit_trig = 5;

	// heating intervals
	const int delay_ms_profile[10] = {300, 300, 300, 300, 300, 300, 300, 300, 300, 300};

	// bme688 error reading retry delay
	const int delay_r_err = 100;

	// skip "sample_every" samples
	const size_t sample_every = 20;

	// length of median buffer (must be uneven)
	const size_t median_buff_len = 11;

	uint8_t c_profile = 0;
	size_t c_samples_unhandled = 0;
	size_t c_samples_omitted = 0;
	size_t c_samples = 0;

	// allocate R_now
	matrix_data R_now_data = matrix_data_mem_init(10, 10, 0);
	uint8_t R_defined = 0;

	// allocate mean and sd vectors
	vector_data mean_now_data = vector_data_mem_init(10, 0);
	vector_data sd_now_data = vector_data_mem_init(10, 0);

	// allocate matrix for median filtering
	matrix_data median_data = matrix_data_mem_init(10, median_buff_len, 0);
	size_t c_median = 0;

	// initialize with 100kB data buffer
	ml_sampler_init(10, 100000);
	for(;;) {
		if(bme688_routine(c_profile)){
			// on last round of reading from profiles, push data for machine learning processing
			if(c_profile == 9) {

				// omit first c_samples_omit_trig samples
				if(c_samples_omitted >= c_samples_omit_trig) {
					c_samples++;

					// pre-sample for median filtering
					if(c_samples > sample_every - median_buff_len) {
						vector_data sample_buff_data = matrix_data_to_vector_data(median_data, c_median, 0);
						vector_data sample_data = vector_to_vector_data((vector_t)bme688_get_p_gas(), 10);
						vector_copy(sample_buff_data, sample_data);
						//vector_print(sample_buff_data, "sampled for median filtering");
						c_median++;
						c_median = c_median%median_buff_len;
					}

					//filter pre collected samples and save median
					if(c_samples >= sample_every) {
						c_samples = 0;
						matrix_data median_T_data = matrix_transpose(median_data, 1);
						//matrix_print(median_T_data, "median_T before heapsort");
						for(size_t u = 0; u < median_data.n_len; u++) {
							heapsort(median_T_data.data[u], median_T_data.m_len);
						}
						//matrix_print(median_T_data, "median_T after heapsort");
						matrix_data median_TT_data = matrix_transpose(median_T_data, 1);
						matrix_free(median_T_data);
						vector_data median_vec_data = matrix_data_to_vector_data(median_TT_data, (median_buff_len/2)+1, 1);
						matrix_free(median_TT_data);
						vector_t v_fil = median_vec_data.data;
						vector_print(median_vec_data, "found median");

						if(!ml_sampler_data_push(v_fil)) {
							printf("Failed to save data\n");
						} else {
							c_samples_unhandled++;
						}
						vector_free(median_vec_data);
					}
				} else {
					c_samples++;
					c_samples_omitted++;
				}
			}

			vTaskDelay(delay_ms_profile[c_profile] / portTICK_PERIOD_MS);

			c_profile++;
			c_profile %= 10;
		} else {
			vTaskDelay(delay_r_err / portTICK_PERIOD_MS);
		}

		if(c_samples_unhandled == c_samples_trig) {
			// Collect all samples
			matrix_data A_data = ml_sampler_data_collect();
			c_samples_unhandled = 0;

			// data normalization
			matrix_data A_norm_data = matrix_data_mem_copy(A_data);
			ml_matrix_normalize(A_norm_data, mean_now_data, sd_now_data);



			//matrix_print(A_data, "A");
			//matrix_print(A_norm_data, "A_norm");

			//Concatenate R and new data
			matrix_data AR_norm_data;
			if(R_defined) {
				AR_norm_data = matrix_data_m_concatenate(R_now_data, A_norm_data);
				matrix_free(A_norm_data);
			}
			else {
				AR_norm_data = A_norm_data;
			}

			matrix_data A_norm_cpy = matrix_data_mem_copy(AR_norm_data);

			// calculate QR decomposition
			matrix_data Q = ml_calc_Q(AR_norm_data);
			R_now_data = ml_calc_R(Q, AR_norm_data);
			R_defined = 1;

			//matrix_print(Q, "Q");
			//matrix_print(R_now_data, "R");

			// process SVD on R
			svd_uwv svd_ret = ml_svd(R_now_data);

			//matrix_print(svd_ret.U, "U");
			vector_print(svd_ret.W, "W");
			matrix_print(svd_ret.V, "V");

			// calculate projection of Axes
			matrix_data P_axis = ml_project_axis(A_norm_cpy, svd_ret.V);
			matrix_print(P_axis, "Axes_T");


			vector_free(svd_ret.W);
			matrix_free(svd_ret.V);
			matrix_free(P_axis);
			matrix_free(Q);
			matrix_free(A_data);
			matrix_free(AR_norm_data);
		}
	}
	return NULL;
}
