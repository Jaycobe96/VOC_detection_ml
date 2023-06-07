/*
 * bme688.c
 *
 *  Created on: Apr 29, 2022
 *      Author: Jakub Tomczak
 */

#include "i2c2_bme688.h"

#include "hw.h"
#include <string.h>
#include "spi2.h"
#include "i2c2.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_pthread.h"
#include "pthread.h"
#include "ml_sampler.h"

#define I2C_BME688_ADR_READ		0x76

static uint16_t bme688_map_heater_ht[10] = {320, 150, 100, 100, 200, 200, 200, 320, 320, 320};
static uint16_t bme688_map_heater_wait[10] = {60, 60, 60, 60, 60, 60, 60, 60, 60, 60};

typedef struct {
	uint8_t init;
	float temp_now;  	// in C
	float hum_now;   	// in %rH
	float press_now; 	// in HPa
	ml_data_type gas_res_now[10];	// in Ohm
}bme688_status_t;

typedef enum {
	PAGE_1 = 0,
	PAGE_0
}mem_page;

typedef struct {
	uint8_t is_calib;
	int32_t temp_comp;
	int32_t t_fine;
	uint16_t par_t1;
	int16_t par_t2;
	int8_t par_t3;
	uint16_t par_h1;
	uint16_t par_h2;
	int8_t par_h3;
	int8_t par_h4;
	int8_t par_h5;
	uint8_t par_h6;
	int8_t par_h7;
	int8_t par_gh1;
	int16_t par_gh2;
	int8_t par_gh3;
	uint16_t par_p1;
	int16_t par_p2;
	int8_t par_p3;
	int16_t par_p4;
	int16_t par_p5;
	int8_t par_p6;
	int8_t par_p7;
	int16_t par_p8;
	int16_t par_p9;
	uint8_t par_p10;
	uint8_t res_heat_range;
	int8_t res_heat_val;
}bme688_calib_param_t;

static bme688_status_t status_now = {0};
static bme688_calib_param_t calib_param_now = {0};

static uint8_t bme688_set_forced(void);
static uint8_t bme688_set_profile(uint8_t profile);
static uint8_t bme688_configure_forced(void);
static uint8_t bme688_configure_parallel(void);
static uint8_t bme688_read_calib_param(bme688_calib_param_t *out);
//static uint8_t bme688_configure(void);
static uint8_t calc_res_heat(uint16_t temp);
static uint8_t bme688_is_data_ready(uint8_t profile_index, uint8_t *profile_read, uint8_t *sub_meas_index, uint8_t *gas_meas_index, uint8_t *gas_valid);


uint8_t bme688_is_i2c(void) {

	uint8_t byte;
	byte = i2c2_read_byte(SPI_BMI688_ADR_VARIANT_ID_I2C, I2C_BME688_ADR_READ);
	return (byte == 0x01) ? byte : 0;
}

void bme688_init(void) {
	uint8_t reset_adr = 0xE0;
	uint8_t reset_byte = 0xB6;

	i2c2_send_byte(reset_adr, reset_byte, I2C_BME688_ADR_READ);
	vTaskDelay(20 / portTICK_PERIOD_MS);

	bme688_read_calib_param(&calib_param_now);

	uint8_t x02_reg = i2c2_read_byte(0x02, I2C_BME688_ADR_READ);
	calib_param_now.res_heat_range = (x02_reg >> 4) & 0x03;
	calib_param_now.res_heat_val = i2c2_read_byte(0x00, I2C_BME688_ADR_READ);
	printf("res_heat_range: %u\n", calib_param_now.res_heat_range);
	printf("x02_reg: %u\n", x02_reg);
	printf("res_heat_val: %i\n", calib_param_now.res_heat_val);

	if(!bme688_configure_forced()) {
		printf("bme_configure_error\n");
		return;
	}
}

float bme688_celcius_now(void) {
	return  status_now.temp_now;
}

float bme688_prH_now(void) {
	return  status_now.hum_now;
}

float bme688_pressure_now(void) {
	return  status_now.press_now;
}

ml_data_type bme688_gas_res_now(uint8_t profile) {
	return status_now.gas_res_now[profile];
}

ml_data_type* bme688_get_p_gas(void) {
	return status_now.gas_res_now;
}

uint8_t bme688_is_init(void) {
	return status_now.init;
}

void bme688_gas_res_print_all(void) {
	for(uint8_t i = 0; i < 10; i++) {
		printf("p%u: %u\n", i, (uint32_t)bme688_gas_res_now(i));
	}
	printf("\n");
}


static uint8_t last_sub_meas_index = 0;
static uint8_t last_gas_meas_index = 0;
static int32_t temperature_now = 0;

uint8_t bme688_routine(uint8_t profile) {
	bme688_status_t new_sample = status_now;
	new_sample.init = 1;

	// Read calibration parameters
	bme688_calib_param_t calib_param = calib_param_now;

	// m<t timeout counter
	uint32_t timeout = 200;

	uint8_t sub_meas_index = 0, gas_meas_index = 0, profile_read = 0, gas_valid = 0;
	// here goes data check
	uint8_t ret = 0;
	uint8_t field_now = 0;

	bme688_set_profile(profile);

	vTaskDelay(5 / portTICK_PERIOD_MS);
	bme688_set_forced();
	vTaskDelay(5 / portTICK_PERIOD_MS);

	while(!ret && timeout) {
		field_now++;
		field_now = field_now%3;
		ret = bme688_is_data_ready(field_now, &profile_read, &sub_meas_index, &gas_meas_index, &gas_valid);
		timeout--;
		vTaskDelay(1 / portTICK_PERIOD_MS);
	}

	for(uint8_t i = 0; i < 10; i++) {
		i2c2_reg_write(SPI_BMI688_ADR_RES_HEAT + i,
			calc_res_heat(bme688_map_heater_ht[i]), I2C_BME688_ADR_READ);
	}

	if(!timeout) {
		printf("bme688 data read fail\n");
		return 0;
	}

	last_sub_meas_index = sub_meas_index;
	last_gas_meas_index = gas_meas_index;

	uint8_t field_adr_incr = field_now * 0x11;

	// Read adc values
	uint8_t adc_p_msb = i2c2_read_byte(0x1f + field_adr_incr, I2C_BME688_ADR_READ);
	uint8_t adc_p_lsb = i2c2_read_byte(0x20 + field_adr_incr, I2C_BME688_ADR_READ);
	uint8_t adc_p_xlsb = i2c2_read_byte(0x21 + field_adr_incr, I2C_BME688_ADR_READ);
	uint8_t adc_t_msb = i2c2_read_byte(0x22 + field_adr_incr, I2C_BME688_ADR_READ);
	uint8_t adc_t_lsb = i2c2_read_byte(0x23 + field_adr_incr, I2C_BME688_ADR_READ);
	uint8_t adc_t_xlsb = i2c2_read_byte(0x24 + field_adr_incr, I2C_BME688_ADR_READ);
	uint8_t adc_h_msb = i2c2_read_byte(0x25 + field_adr_incr, I2C_BME688_ADR_READ);
	uint8_t adc_h_lsb = i2c2_read_byte(0x26 + field_adr_incr, I2C_BME688_ADR_READ);
	uint8_t adc_g_lsb = i2c2_read_byte(0x2d + field_adr_incr, I2C_BME688_ADR_READ);
	uint8_t adc_g_msb = i2c2_read_byte(0x2c + field_adr_incr, I2C_BME688_ADR_READ);


	// Calculate temp
	{
			uint32_t temp_adc = 0;
			temp_adc = PARSE_UINT32_20BIT(adc_t_xlsb,
					adc_t_lsb, adc_t_msb);
	    int64_t var1;
	    int64_t var2;
	    int64_t var3;
	    int16_t calc_temp;

	    var1 = ((int32_t)temp_adc >> 3) - ((int32_t)calib_param.par_t1 << 1);
	    var2 = (var1 * (int32_t)calib_param.par_t2) >> 11;
	    var3 = ((var1 >> 1) * (var1 >> 1)) >> 12;
	    var3 = ((var3) * ((int32_t)calib_param.par_t3 << 4)) >> 14;
	    calib_param.t_fine = var2 + var3;
	    calc_temp = ((calib_param.t_fine * 5) + 128) >> 8;
	    temperature_now = (((float) calc_temp) / 100);
	}

	//Calculate Humidity
	{
			uint16_t adc_hum = PARSE_UINT16(adc_h_lsb, adc_h_msb);


			int32_t temp_scaled = (((int32_t)calib_param.t_fine * 5) + 128) >> 8;
			int32_t var1 = (int32_t)(adc_hum - ((int32_t)((int32_t)calib_param.par_h1 * 16))) -
					(((temp_scaled * (int32_t)calib_param.par_h3) / ((int32_t)100)) >> 1);
			int32_t var2 = ((int32_t)calib_param.par_h2 *
					(((temp_scaled * (int32_t)calib_param.par_h4) / ((int32_t)100)) +
							(((temp_scaled * ((temp_scaled * (int32_t)calib_param.par_h5) / ((int32_t)100))) >> 6) / ((int32_t)100)) +
							(int32_t)(1 << 14))) >> 10;
			int32_t var3 = var1 * var2;
			int32_t var4 = (int32_t)calib_param.par_h6 << 7;
			var4 = ((var4) + ((temp_scaled * (int32_t)calib_param.par_h7) / ((int32_t)100))) >> 4;
			int32_t var5 = ((var3 >> 14) * (var3 >> 14)) >> 10;
			int32_t var6 = (var4 * var5) >> 1;
			int32_t calc_hum = (((var3 + var6) >> 10) * ((int32_t)1000)) >> 12;

			calc_hum = calc_hum > 100000 ? 100000 : calc_hum;
			calc_hum = calc_hum < 0 ? 0 : calc_hum;

			if(calc_hum) {
				new_sample.hum_now = (float) calc_hum / 1000;
			} else {
				new_sample.hum_now = 0;
			}
	}

	//Calculate Pressure
	{

			uint32_t press_raw = PARSE_UINT32_20BIT(adc_p_xlsb,
					adc_p_lsb, adc_p_msb);

			int32_t var1 = ((int32_t)calib_param.t_fine >> 1) - 64000;
			int32_t var2 = ((((var1 >> 2) * (var1 >> 2)) >> 11) * (int32_t)calib_param.par_p6) >> 2;
			var2 = var2 + ((var1 * (int32_t)calib_param.par_p5) << 1);
			var2 = (var2 >> 2) + ((int32_t)calib_param.par_p4 << 16);
			var1 = (((((var1 >> 2) * (var1 >> 2)) >> 13) *
					((int32_t)calib_param.par_p3 << 5)) >> 3) + (((int32_t)calib_param.par_p2 * var1) >> 1);
			var1 = var1 >> 18;
			var1 = ((32768 + var1) * (int32_t)calib_param.par_p1) >> 15;
			int32_t press_comp = 1048576 - press_raw;
			press_comp = (uint32_t)((press_comp - (var2 >> 12)) * ((uint32_t)3125));
			if(press_comp >= (1 << 30)) {
				press_comp = ((press_comp / (uint32_t)var1) << 1);
			} else if(var1) {
				press_comp = ((press_comp << 1) / (uint32_t)var1);

			}
			var1 = ((int32_t)calib_param.par_p9 * (int32_t)(((press_comp >> 3) *
					(press_comp >> 3)) >> 13)) >> 12;
			var2 = ((int32_t)(press_comp >> 2) * (int32_t)calib_param.par_p8) >> 13;
			int32_t var3 = ((int32_t)(press_comp >> 8) * (int32_t)(press_comp >> 8) *
					(int32_t)(press_comp >> 8) * (int32_t)calib_param.par_p10) >> 17;
			press_comp = (int32_t)(press_comp) +
					((var1 + var2 + var3 + ((int32_t)calib_param.par_p7 << 7)) >> 4);

			new_sample.press_now = (float) press_comp / 100;
	}

	//Calculate gas res
	{
			uint16_t gas_res_adc = PARSE_UINT16_10BIT(adc_g_lsb, adc_g_msb);
			uint8_t gas_range = adc_g_lsb & 0x0f;
			uint32_t var1 = 262144u >> gas_range;
			int32_t var2 = (int32_t)gas_res_adc - 512;
			var2 *= 3;
			var2 = 4096 + var2;

			uint32_t calc_gas_res = (double)(1000000.0f * (float)var1 / (float)var2);
	    /* multiplying 10000 then dividing then multiplying by 100 instead of multiplying by 1000000 to prevent overflow */
		/*	uint32_t calc_gas_res = (10000u * var1) / (uint32_t)var2;
			calc_gas_res = calc_gas_res * 100;
		 	 */
			new_sample.gas_res_now[profile_read] = (ml_data_type) calc_gas_res;

			// UNCOMMENT FOR gas output in terminal
			//printf("p%u: %u\n", profile_read, calc_gas_res);
	}

		// update all data on successful data read
		status_now.gas_res_now[profile_read] = new_sample.gas_res_now[profile_read];
		status_now.temp_now = temperature_now;

	return 1;
}

static uint8_t bme688_is_data_ready(uint8_t profile_index, uint8_t *profile_read, uint8_t *sub_meas_index, uint8_t *gas_meas_index, uint8_t *gas_valid) {
	uint8_t index_meas_status = 0x1d + 0x11 * profile_index;
	uint8_t index_sub_meas_index = 0x1e + 0x11 * profile_index;
	uint8_t index_gas_r_lsb = 0x2d + 0x11 * profile_index;
	uint8_t meas_status = i2c2_read_byte(index_meas_status, I2C_BME688_ADR_READ);
	uint8_t sub_meas_index_now = i2c2_read_byte(index_sub_meas_index, I2C_BME688_ADR_READ);
	uint8_t meas_status_new_data = (meas_status >> 7) & 1;
	uint8_t meas_status_gas_measuring_index = meas_status & 0x0F;
	uint8_t gas_r_lsb = i2c2_read_byte(index_gas_r_lsb, I2C_BME688_ADR_READ);
	uint8_t gas_valid_now =	(gas_r_lsb >> 5) & 1u;
	uint8_t gas_heat_stab =	(gas_r_lsb >> 4) & 1u;
	uint8_t profile_out = meas_status & 0x0F;

	if(meas_status_new_data && gas_heat_stab && gas_valid_now) {
		*profile_read = profile_out;
		*sub_meas_index = sub_meas_index_now;
		*gas_meas_index = meas_status_gas_measuring_index;
		*gas_valid = gas_valid_now;
		return 1;
	}
	return 0;
}

static uint8_t bme688_read_calib_param(bme688_calib_param_t *out) {
	bme688_calib_param_t new_calib;

	//Temperature params
	uint8_t par_t2_lsb = i2c2_read_byte(0x8A, I2C_BME688_ADR_READ);
	uint8_t par_t2_msb = i2c2_read_byte(0x8B, I2C_BME688_ADR_READ);
	new_calib.par_t2 = PARSE_UINT16(par_t2_lsb, par_t2_msb);
	new_calib.par_t3 = i2c2_read_byte(0x8C, I2C_BME688_ADR_READ);
	uint8_t par_t1_lsb = i2c2_read_byte(0xe9, I2C_BME688_ADR_READ);
	uint8_t par_t1_msb = i2c2_read_byte(0xea, I2C_BME688_ADR_READ);
	new_calib.par_t1 = PARSE_UINT16(par_t1_lsb, par_t1_msb);

	//Humidity params
	uint8_t par_msb = i2c2_read_byte(0xe3, I2C_BME688_ADR_READ);
	uint8_t par_lsb = i2c2_read_byte(0xe2, I2C_BME688_ADR_READ) & 0x0f;
	new_calib.par_h1 = PARSE_UINT16_10B_MSBSHIFT(par_lsb, par_msb);
	par_msb = i2c2_read_byte(0xe1, I2C_BME688_ADR_READ);
	par_lsb = i2c2_read_byte(0xe2, I2C_BME688_ADR_READ) & 0xf0;
	new_calib.par_h2 = PARSE_UINT16(par_lsb, par_msb) >> 4;
	new_calib.par_h3 = i2c2_read_byte(0xe4, I2C_BME688_ADR_READ);
	new_calib.par_h4 = i2c2_read_byte(0xe5, I2C_BME688_ADR_READ);
	new_calib.par_h5 = i2c2_read_byte(0xe6, I2C_BME688_ADR_READ);
	new_calib.par_h6 = i2c2_read_byte(0xe7, I2C_BME688_ADR_READ);
	new_calib.par_h7 = i2c2_read_byte(0xe8, I2C_BME688_ADR_READ);

	//Pressure params
	par_msb = i2c2_read_byte(0x8f, I2C_BME688_ADR_READ);
	par_lsb = i2c2_read_byte(0x8e, I2C_BME688_ADR_READ);
	new_calib.par_p1 = PARSE_UINT16(par_lsb, par_msb);
	par_msb = i2c2_read_byte(0x91, I2C_BME688_ADR_READ);
	par_lsb = i2c2_read_byte(0x90, I2C_BME688_ADR_READ);
	new_calib.par_p2 = PARSE_UINT16(par_lsb, par_msb);
	new_calib.par_p3 = i2c2_read_byte(0x92, I2C_BME688_ADR_READ);
	par_msb = i2c2_read_byte(0x95, I2C_BME688_ADR_READ);
	par_lsb = i2c2_read_byte(0x94, I2C_BME688_ADR_READ);
	new_calib.par_p4 = PARSE_UINT16(par_lsb, par_msb);
	par_msb = i2c2_read_byte(0x97, I2C_BME688_ADR_READ);
	par_lsb = i2c2_read_byte(0x96, I2C_BME688_ADR_READ);
	new_calib.par_p5 = PARSE_UINT16(par_lsb, par_msb);
	new_calib.par_p6 = i2c2_read_byte(0x99, I2C_BME688_ADR_READ);
	new_calib.par_p7 = i2c2_read_byte(0x98, I2C_BME688_ADR_READ);
	par_msb = i2c2_read_byte(0x9d, I2C_BME688_ADR_READ);
	par_lsb = i2c2_read_byte(0x9c, I2C_BME688_ADR_READ);
	new_calib.par_p8 = PARSE_UINT16(par_lsb, par_msb);
	par_msb = i2c2_read_byte(0x9f, I2C_BME688_ADR_READ);
	par_lsb = i2c2_read_byte(0x9e, I2C_BME688_ADR_READ);
	new_calib.par_p9 = PARSE_UINT16(par_lsb, par_msb);
	new_calib.par_p10 = i2c2_read_byte(0xa0, I2C_BME688_ADR_READ);

	//Heater params
	new_calib.par_gh1 = i2c2_read_byte(0xED, I2C_BME688_ADR_READ);
	par_lsb = i2c2_read_byte(0xEB, I2C_BME688_ADR_READ);
	par_msb = i2c2_read_byte(0xEC, I2C_BME688_ADR_READ);
	new_calib.par_gh3 = i2c2_read_byte(0xEE, I2C_BME688_ADR_READ);
	new_calib.par_gh2 = PARSE_UINT16(par_lsb, par_msb);

	*out = new_calib;
	return 1;
}

static uint8_t calc_res_heat(uint16_t temp)
{
    double var1;
    double var2;
    double var3;
    double var4;
    double var5;
    double amb_temp = 25.0;
    uint8_t res_heat;

    if (temp > 400) /* Cap temperature */
    {
        temp = 400;
    }

    var1 = ((double)calib_param_now.par_gh1 / 16.0f) + 49.0f;
    var2 = (((double)calib_param_now.par_gh2 / (32768.0f)) * (0.0005f)) + 0.00235f;
    var3 = (double)calib_param_now.par_gh3 / (1024.0f);
    var4 = (var1 * (1.0f + (var2 * (double)temp)));
    var5 = (var4 + (var3 * amb_temp));
    res_heat =
        (uint8_t)(3.4f *
                  ((var5 * (4.0f / (4.0f + (double)calib_param_now.res_heat_range)) *
                    (1.0f / (1.0f + ((double)calib_param_now.res_heat_val * 0.002f)))) -
                   25.00));

    return res_heat;
}

static uint8_t bme688_set_forced(void) {
	return i2c2_reg_write(SPI_BMI688_ADR_CTRL_MEAS,
			SPI_BMI688_DAT_CTRL_MEAS_FORCED, I2C_BME688_ADR_READ);
}

static uint8_t bme688_set_profile(uint8_t profile) {
	return i2c2_reg_append(SPI_BMI688_ADR_CTRL_GAS_1,
			SPI_BMI688_DAT_CTRL_GAS_1_RUN_GAS + profile, I2C_BME688_ADR_READ, SPI_BMI688_DAT_CTRL_GAS_1_MASK);
}


static uint8_t bme688_configure_forced(void) {

	uint8_t spi_ret = i2c2_reg_append(SPI_BMI688_ADR_CTRL_HUM,
			SPI_BMI688_DAT_CTRL_HUM, I2C_BME688_ADR_READ, SPI_BMI688_ADR_CTRL_HUM_MASK);

	spi_ret &= i2c2_reg_append(SPI_BMI688_ADR_CTRL_MEAS,
			SPI_BMI688_DAT_CTRL_MEAS, I2C_BME688_ADR_READ, SPI_BMI688_ADR_CTRL_MEAS_MASK);

	spi_ret &= i2c2_reg_append(SPI_BMI688_ADR_CONFIG,
			SPI_BMI688_DAT_FILTER, I2C_BME688_ADR_READ, SPI_BMI688_ADR_FILTER_MASK);


	// set gas wait for all profiles
	for(uint8_t i = 0; i < 10; i++) {
		spi_ret &= i2c2_reg_write(SPI_BMI688_ADR_GAS_WAIT + i,
				bme688_map_heater_wait[i], I2C_BME688_ADR_READ);

		spi_ret &= i2c2_reg_write(SPI_BMI688_ADR_RES_HEAT + i,
				calc_res_heat(bme688_map_heater_ht[i]), I2C_BME688_ADR_READ);
	}

	// enable gas and nb_conv=0
	spi_ret &= bme688_set_profile(0);

	// ensure heating in on
	spi_ret &= i2c2_reg_append(SPI_BMI688_ADR_CTRL_GAS_0,
			SPI_BMI688_DAT_CTRL_GAS_0_RUN_GAS, I2C_BME688_ADR_READ, SPI_BMI688_DAT_CTRL_GAS_0_MASK);

	//set to forced mode
	spi_ret &= bme688_set_forced();
	return spi_ret;
}

static uint8_t bme688_configure_parallel(void) {

	//set H oversampling, 3 wire interupt off
	uint8_t res = i2c2_reg_append(SPI_BMI688_ADR_CTRL_HUM,
			OSRS_H_VAL, I2C_BME688_ADR_READ, 0x47);

	// set TP oversampling and sleep mode
	res &= i2c2_reg_write(SPI_BMI688_ADR_CTRL_MEAS,
			OSRS_T_VAL | OSRS_P_VAL | MODE_SLEEP,
			I2C_BME688_ADR_READ);

	// set Filter, off 3-wire spi
	res &= i2c2_reg_append(SPI_BMI688_ADR_CONFIG,
			SPI_BMI688_DAT_FILTER, I2C_BME688_ADR_READ, 0x1D);

	// enable gas and nb_conv=0
	res &= i2c2_reg_append(SPI_BMI688_ADR_CTRL_GAS_1,
			SPI_BMI688_DAT_CTRL_GAS_1_RUN_GAS, I2C_BME688_ADR_READ, SPI_BMI688_DAT_CTRL_GAS_1_MASK);

	// set gas wait for plate 0
	for(uint8_t i = 0; i < 10; i++) {
		res &= i2c2_reg_write(SPI_BMI688_ADR_GAS_WAIT + i,
				bme688_map_heater_wait[i], I2C_BME688_ADR_READ);

		res &= i2c2_reg_write(SPI_BMI688_ADR_RES_HEAT + i,
				calc_res_heat(bme688_map_heater_ht[i]), I2C_BME688_ADR_READ);
	}

	// ensure heating in on
	res &= i2c2_reg_append(SPI_BMI688_ADR_CTRL_GAS_0,
			SPI_BMI688_DAT_CTRL_GAS_0_RUN_GAS, I2C_BME688_ADR_READ, SPI_BMI688_DAT_CTRL_GAS_0_MASK);

	// set parallel mode
	res &= i2c2_reg_write(SPI_BMI688_ADR_CTRL_MEAS,
			OSRS_T_VAL | OSRS_P_VAL | MODE_PARALLEL,
			I2C_BME688_ADR_READ);

	return res;
}
