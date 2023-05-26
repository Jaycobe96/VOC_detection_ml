#include "sampler_ctrl.h"
#include "sensordata_wrapper.h"
#include "bluetooth_wrapper.h"
#include "sampler.h"
#include "pthread.h"
#include "esp_pthread.h"
#include "esp_system.h"
#include "esp_event.h"

// TODO: spi2 include parsing macros which is wrong place for macros
#include "spi2.h"

#define SAMPLER_CTRL_DATA_SEGMENT_SIZE 10

typedef enum{
	SAMPLER_CTRL_STATE_NO_COMMAND = 0,
	SAMPLER_CTRL_STATE_START_SAMPLER,
	SAMPLER_CTRL_STATE_RESUME_SAMPLER,
	SAMPLER_CTRL_STATE_SET_CLASS_NAME,
	SAMPLER_CTRL_STATE_SET_COOLDOWN,
	SAMPLER_CTRL_STATE_RETURN_CLASS_NAMES,
	SAMPLER_CTRL_STATE_BUFFERED_DATA_REQUEST,
	SAMPLER_CTRL_STATE_REQUEST_DATA_SEGMENT
}sampler_state;

static sampler_state sampler_ctrl_state = SAMPLER_CTRL_STATE_NO_COMMAND;
static uint8_t buff_command[10] = {0};

static pthread_t process_sampler_ctrl_thread = 0;
static const esp_pthread_cfg_t process_sampler_ctrl_cfg = {
		.stack_size = 32768,
		.prio = 2,
		.inherit_cfg = false,
};

static uint8_t **buff_ml_pages = NULL;
static size_t buff_ml_page_count = 0;
static float sampler_ctrl_freq_now = (0.5);

static void *process_sampler_ctrl(void *arg) {
	for(;;) {
		//IN
		//find out at what state sampler is
		uint8_t status[10] = {0};
		status[0] = 250u;
		if(sensordata_is_sampler_running()) {
			status[1] = 1u;
		} else if(sensordata_is_requesting_new_class()) {
			status[1] = 2u;
		}
		uint32_t sample_count = sampler_get_sample_count();
		status[2] = UINT32_TO_UINT8(sample_count, 0);
		status[3] = UINT32_TO_UINT8(sample_count, 1);
		bt_gatt_send(status, 10);

		//OUT
		bt_get_out_characteristic(buff_command);
		if(buff_command[0] <= 0x10) {
			vTaskDelay((uint32_t)((float) (1000.0 / sampler_ctrl_freq_now / portTICK_PERIOD_MS)));
			continue;
		}
		sampler_ctrl_state = buff_command[0];

		uint8_t decode_offset = 0x10;
		switch (sampler_ctrl_state - decode_offset) {
			case SAMPLER_CTRL_STATE_NO_COMMAND: break;

			case (SAMPLER_CTRL_STATE_START_SAMPLER): {
				uint8_t n_class = buff_command[1],
						tot_len = buff_command[2],
						interval = buff_command[3];
				sensordata_set_sampler(tot_len, 12, interval, n_class);
				break;
			}
			case (SAMPLER_CTRL_STATE_RESUME_SAMPLER):
					sensordata_new_class_request_clear();
				sensordata_resume_sampler();
				break;
			case (SAMPLER_CTRL_STATE_BUFFERED_DATA_REQUEST): {
				if(buff_ml_pages != NULL) {
					bt_gatt_send_page(buff_ml_pages, 0);

				}
				break;
			};
			case (SAMPLER_CTRL_STATE_REQUEST_DATA_SEGMENT): {
				if(buff_ml_pages != NULL) {
					assert(buff_command[1] < buff_ml_page_count);
					size_t page =  PARSE_UINT16(buff_command[2], buff_command[1]);

					for(uint16_t i = page; i < page + SAMPLER_CTRL_DATA_SEGMENT_SIZE; i++) {
						if(i < (sensordata_return_sampler_size() / 16)) {
							bt_gatt_send_page(buff_ml_pages, i);
						}
					}
					printf("Segment sent: page%u\n", page);
				}

				break;
			}
			default:
				break;
		}
		bt_clear_out_characteristic();
		vTaskDelay((uint32_t)((float) (1000.0 / sampler_ctrl_freq_now / portTICK_PERIOD_MS)));
	}
	return NULL;
}

void sampler_ctrl_thd_start(float op_freq) {
	int res;
	assert(op_freq > 0.0);
	esp_pthread_set_cfg(&process_sampler_ctrl_cfg);
	sampler_ctrl_freq_now = op_freq;
	res = pthread_create(&process_sampler_ctrl_thread, NULL, process_sampler_ctrl, NULL);
	assert(res == 0);
}


void sampler_ctrl_set_ml_page_buff(uint8_t** val, size_t page_count) {
	assert(val != NULL);
	if(buff_ml_pages != NULL) {
		for(size_t i = 0; i < buff_ml_page_count; i++) {
			free(buff_ml_pages[i]);
		}
		free(buff_ml_pages);
	}
	buff_ml_pages = val;
	buff_ml_page_count = page_count;
}


