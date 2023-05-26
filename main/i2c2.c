/*
 * i2c.c
 *
 *  Created on: 23 Sep 2022
 *      Author: s174873
 */

#include "i2c2.h"
#include "driver/i2c.h"
#include "esp_system.h"
#include "esp_event.h"

#include "hw.h"

i2c_config_t i2c2_cfg = {
		 .mode = I2C_MODE_MASTER,
		 .sda_io_num = HW_I2C_BME688_SDA,
		 .scl_io_num = HW_I2C_BME688_SCL,
		 .sda_pullup_en = true,
		 .scl_pullup_en = true,
		 .master.clk_speed = 500000,
		 .clk_flags = I2C_SCLK_SRC_FLAG_FOR_NOMAL
};


void i2c2_init(void) {
	ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_param_config(I2C_NUM_1, &i2c2_cfg));
	ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_driver_install(I2C_NUM_1, I2C_MODE_MASTER, 0, 0, 0));
}

uint8_t i2c2_send_byte(uint8_t reg, uint8_t data, uint8_t adr) {
	uint8_t wr_buff[2] =  {reg, data};
	ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_write_to_device(I2C_NUM_1, adr, wr_buff, 2, 100 / portTICK_PERIOD_MS));

	/*i2c_cmd_handle_t cmd_handle = i2c_cmd_link_create();
	ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_start(cmd_handle));
	ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_write_byte(cmd_handle, adr, true));
	ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_write(cmd_handle, wr_buff, 2, true));
	ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_stop(cmd_handle));

	ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_cmd_begin(I2C_NUM_1, cmd_handle, 100 / portTICK_PERIOD_MS));
	i2c_cmd_link_delete(cmd_handle);*/
	return 1;
}

uint8_t i2c2_read_byte(uint8_t reg, uint8_t adr) {
	uint8_t out = 0;
	uint8_t test = reg;
	ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_write_read_device(I2C_NUM_1, adr, &test, 1, &out, 1, 100 / portTICK_PERIOD_MS));

	/*i2c_cmd_handle_t cmd_handle = i2c_cmd_link_create();

	// write register
	ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_start(cmd_handle));
	ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_write_byte(cmd_handle, adr, true));
	ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_write(cmd_handle, &reg, 1, true));
	// read register
	ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_start(cmd_handle));
	ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_write_byte(cmd_handle, adr | 0x01, true));
	ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_read(cmd_handle, &out, 1, I2C_MASTER_NACK));
	ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_stop(cmd_handle));

	ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_cmd_begin(I2C_NUM_1, cmd_handle, 100 / portTICK_PERIOD_MS));
	i2c_cmd_link_delete(cmd_handle);*/
	return out;
}

uint8_t i2c2_reg_write(uint8_t reg, uint8_t data, uint8_t adr) {
	i2c2_send_byte(reg, data, adr);
	return (data == i2c2_read_byte(reg, adr)) ? 1 : 0;
}

uint8_t i2c2_reg_append(uint8_t reg, uint8_t data, uint8_t adr, uint8_t mask) {
	uint8_t feedback = i2c2_read_byte(reg, adr);
	feedback &= ~mask;
	uint8_t set = feedback | data;
	return i2c2_reg_write(reg, set, adr);
}

uint8_t i2c2_reg_append_wait(uint8_t reg, uint8_t data, uint8_t adr, uint8_t mask, uint8_t ms_wait) {
	uint8_t feedback = i2c2_read_byte(reg, adr);
	vTaskDelay(ms_wait / portTICK_PERIOD_MS);
	feedback &= ~mask;
	uint8_t set = feedback | data;
	return i2c2_reg_write(reg, set, adr);
}
