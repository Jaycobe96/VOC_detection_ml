#define BME688_USE_I2C2

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include "esp_system.h"
#include "esp_event.h"
#include "sampler.h"
#include "sampler_ctrl.h"

#include "hw.h"
#include "bluetooth_wrapper.h"
#ifdef BME688_USE_I2C2
#include "i2c2.h"
#include "i2c2_bme688.h"
#else
#include "spi2.h"
#include "spi2_bme688.h"
#endif
#include "sensordata_wrapper.h"
#include "ml_process.h"

/*
static uint16_t count_print = 0;
static const count_print_max = 10;
void bme688_print(void) {
    if(count_print >= count_print_max) {
    	count_print = 0;
    	bme688_gas_res_print_all();

}
*/


void app_main(void) {

	i2c2_init();


	bme688_init();
	ml_process_thd_start();
	vTaskDelay(300 / portTICK_PERIOD_MS);

	bt_init();
	sensordata_thd_start();
	sampler_ctrl_thd_start(10);

    while (true) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}
