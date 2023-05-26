/*
 * sampler.c
 *
 *  Created on: 5 Oct 2022
 *      Author: JaycobePC
 */

#include "sampler.h"

#include "string.h"
#include "stdio.h"
#include "sensordata_wrapper.h"
#include "esp_heap_caps.h"

static bool	sampler_is_init = false;
static uint32_t *samples = NULL;
static uint32_t *last_samples = NULL;
static size_t sampler_size = 0;
static size_t last_samples_size = 0;

static uint32_t sampler_N_now = SAMPLER_N,
				sampler_M_now = SAMPLER_M;

static uint32_t p_data = 0;

bool is_sampler_init(void) {
	return sampler_is_init;
}

void sampler_deinit(void) {
	sampler_N_now = SAMPLER_N;
	sampler_M_now = SAMPLER_M;

	if(samples != NULL) {
		free(samples);
		samples = NULL;
	}
}

void sampler_init(uint32_t sampler_N, uint32_t sampler_M) {
	if(!sampler_N) {
		sampler_N_now = SAMPLER_N;
	} else {
		sampler_N_now = sampler_N;
	}

	if(!sampler_M) {
		sampler_M_now = SAMPLER_M;
	} else {
		sampler_M_now = sampler_M;
	}

	if(samples != NULL) {
		free(samples);
		samples = NULL;
	}

	size_t sampler_size_now = sizeof(uint32_t) * sampler_N_now * sampler_M_now;

	samples = (uint32_t*) heap_caps_malloc(sampler_size_now, MALLOC_CAP_32BIT);

	if(sampler_size_now != sampler_size) {

		if(last_samples != NULL) {
			free(last_samples);
			last_samples = NULL;
		}

		last_samples = (uint32_t*) heap_caps_malloc(sampler_size_now, MALLOC_CAP_32BIT);
		assert(last_samples != NULL);
		memset(last_samples, 0, sampler_size_now);
	}

	assert(samples != NULL && last_samples != NULL);
	p_data = 0;
	memset(samples, 0, sampler_size_now);
	sampler_size = sampler_size_now;
	last_samples_size = sampler_size_now;
	sampler_is_init = true;
}

void sampler_add_sample(uint32_t* sensor_data) {
	assert(samples != NULL);

	if(p_data * sizeof(uint32_t) >= sampler_size - 1) {
		printf("sampler overrun\n");
		return;
	}

	memcpy(samples + p_data, sensor_data, sampler_M_now * sizeof(uint32_t));
	p_data += sampler_M_now;
}

void sampler_clear(void) {
	assert(samples != NULL);
	memset(samples, 0, sampler_size);
	p_data = 0;
}

bool sampler_is_full(void) {
	assert(samples != NULL);
	return (p_data * sizeof(uint32_t) >= sampler_size - 1);
}

uint32_t sampler_get_sample_count(void) {
	if(p_data) {
		return p_data / sampler_M_now;
	}
	return 0;
}

size_t sampler_get_size(void) {
	return sampler_size;
}

size_t sampler_get_buffered_size(void) {
	return last_samples_size;
}

uint32_t sampler_get_n(void) {
	return sampler_N_now;
}

uint32_t sampler_get_m(void) {
	return sampler_M_now;
}


void sampler_put_at(uint32_t* sensor_data, uint32_t at_N) {
	assert(samples != NULL);
	memcpy(samples + at_N*sampler_M_now, sensor_data, sampler_M_now * sizeof(uint32_t));
}

void sampler_dump_data(uint32_t* out) {
	assert(samples != NULL && last_samples != NULL);
	memcpy(last_samples, samples, sampler_size);
	memcpy(out, samples, sampler_size);
	sampler_clear();
}

void sampler_dump_raw_data(uint8_t* out) {
	assert(samples != NULL && last_samples != NULL);
	memcpy(last_samples, samples, sampler_size);
	memcpy(out, samples, sampler_size);
	sampler_clear();
	printf("data dumped\n");
}

void sampler_copy_buff_data(uint8_t* out) {
	assert(samples != NULL && last_samples != NULL);
	memcpy(out, last_samples, sampler_size);
}
/*
void samapler_get_first_sample_of_class(uint32_t* out, uint32_t i_class) {
	assert(samples != NULL);
	uint32_t i = sampler_M_now - 1; // last attribute which is class index

	while(i < sampler_size) {
		if(samples[i] == i_class) {
			memcpy(out, samples[i], sampler_M_now);
		}
		i += sampler_M_now;
	}
}
*/


