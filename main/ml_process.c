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

	const size_t c_samples_trig = 20;
	const size_t c_samples_omit_trig = 2;
	const int delay_ms_profile[10] = {300, 300, 300, 300, 300, 300, 300, 300, 300, 300};
	const int delay_r_err = 100;
	const size_t sample_every = 1;

	uint8_t c_profile = 0;
	size_t c_samples_unhandled = 0;
	size_t c_samples_omitted = 0;
	size_t c_samples_every = 0;
	// initialize with 100kB data buffer
	ml_sampler_init(10, 100000);
	for(;;) {
		if(bme688_routine(c_profile)){
			// on last round of reading from profiles, push data for machine learning processing
			if(c_profile == 9) {

				// omit first c_samples_omit_trig samples
				if(c_samples_omitted >= c_samples_omit_trig) {
					c_samples_every++;
					if(c_samples_every >= sample_every) {
						c_samples_every = 0;
						if(!ml_sampler_data_push(bme688_get_p_gas())) {
							printf("Failed to save data\n");
						} else {
							c_samples_unhandled++;
						}
					}
				} else {
					c_samples_every++;
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
			ml_matrix_normalize(A_norm_data);

			matrix_data Test_data = matrix_data_mem_init(3, 3, 0);
			matrix_t test = Test_data.data;
			test[0][0] = 12.0;
			test[0][1] = -51.0;
			test[0][2] = 4.0;
			test[1][0] = 6.0;
			test[1][1] = 167.0;
			test[1][2] = -68.0;
			test[2][0] = -4.0;
			test[2][1] = 24.0;
			test[2][2] = -41.0;

			matrix_print(A_data, "A");
			matrix_print(A_norm_data, "A_norm");
			matrix_data A_norm_cpy = matrix_data_mem_copy(A_norm_data);

			matrix_data Q = ml_calc_Q(A_norm_data);
			matrix_data R = ml_calc_R(Q, A_norm_data);

			matrix_print(Q, "Q");
			matrix_print(R, "R");

			svd_uwv svd_ret = ml_svd(A_norm_data);
			svd_uwv svd_ret_R = ml_svd(R);

			matrix_print(svd_ret.U, "U");
			vector_print(svd_ret.W, "W");
			matrix_print(svd_ret.V, "V");

			matrix_print(svd_ret_R.U, "Ur");
			vector_print(svd_ret_R.W, "Wr");
			matrix_print(svd_ret_R.V, "Vr");

			matrix_data V_t = matrix_transpose(svd_ret.V, 1);
			matrix_data P_axis = ml_project_axis(A_norm_cpy, svd_ret.V);

			//matrix_data P_T_axis = matrix_transpose(P_axis, 1);
			matrix_print(P_axis, "Axis");

			vector_free(svd_ret.W);
			matrix_free(svd_ret.V);
			matrix_free(P_axis);
			matrix_free(V_t);
			matrix_free(Q);
			matrix_free(R);

			matrix_free(A_data);
			matrix_free(A_norm_data);
		}
	}
	return NULL;
}
