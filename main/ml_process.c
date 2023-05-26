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

	const size_t c_samples_trig = 6;
	const int delay_ms_profile[10] = {300, 300, 300, 300, 300, 300, 300, 300, 300, 300};
	const int delay_r_err = 100;

	uint8_t c_profile = 0;
	size_t c_samples_unhandled = 0;
	// initialize with 100kB data buffer
	ml_sampler_init(10, 100000);
	for(;;) {
		if(bme688_routine(c_profile)){
			// on last round of reading from profiles, push data for machine learning processing
			if(c_profile == 9) {
				if(!ml_sampler_data_push(bme688_get_p_gas())) {
					printf("Failed to save data\n");
				} else {
					c_samples_unhandled++;
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
			A_data.n_len = 4;
			c_samples_unhandled = 0;

			// Process machine learning
			matrix_data A_norm_data = matrix_data_mem_copy(A_data);
			vector_data v_diff = vector_data_mem_init(A_norm_data.n_len, 0);
			ml_matrix_normalize(A_norm_data, &v_diff);

			matrix_print(A_data, "A");
			matrix_print(A_norm_data, "A_norm");
			vector_print(v_diff, "attr_diff");

			matrix_data A_norm_t_data = matrix_transpose(A_norm_data, 1);

			matrix_data V_data = matrix_mult(A_norm_t_data, A_norm_data);
			matrix_print(V_data, "V");

			matrix_data D_data = ml_tridiagonalization(V_data);
			matrix_print(D_data, "D");

			matrix_free(A_data);
			matrix_free(A_norm_data);
			matrix_free(A_norm_t_data);
			matrix_free(V_data);
			vector_free(v_diff);
		}
	}
	return NULL;
}

/*
 * 		// mult test
		matrix_t A = matrix_mem_init(3, 2, 0);
		matrix_t B = matrix_mem_init(2, 3, 0);
		A[0][0] = 1.0;
		A[0][1] = 2.0;
		A[0][2] = 3.0;

		A[1][0] = 4.0;
		A[1][1] = 5.0;
		A[1][2] = 6.0;

		B[0][0] = 10.0;
		B[1][0] = 20.0;
		B[2][0] = 30.0;

		B[0][1] = 11.0;
		B[1][1] = 21.0;
		B[2][1] = 31.0;

		matrix_data A_data = matrix_to_matrix_data(A, 3, 2);
		matrix_data B_data = matrix_to_matrix_data(B, 2, 3);

		matrix_data C_data = matrix_mult(A_data, B_data);
		matrix_print(C_data);
 */
