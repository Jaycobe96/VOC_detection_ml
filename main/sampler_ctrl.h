#ifndef MAIN_SAMPLER_CTRL_H_
#define MAIN_SAMPLER_CTRL_H_

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

void sampler_ctrl_set_ml_page_buff(uint8_t** val, size_t page_count);
void sampler_ctrl_thd_start(float op_freq);

#endif
