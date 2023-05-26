/*
 * bme688.c
 *
 *  Created on: Apr 29, 2022
 *      Author: Jakub Tomczak
 */

#include "spi2_bme688.h"

#include "hw.h"
#include <string.h>
#include "spi2.h"
#include "esp_system.h"
#include "esp_event.h"

typedef enum {
	PAGE_1 = 0,
	PAGE_0
}mem_page;

typedef struct {
	uint8_t init;
	float temp_now;  	// in C
	float hum_now;   	// in %rH
	float press_now; 	// in HPa
	float gas_res_now[10];	// in Ohm
}bme688_status_t;

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

static uint8_t bme688_mem_page_set(mem_page page);
//static uint8_t bme688_configure_forced(void);
static uint8_t bme688_configure_parallel(void);
static uint8_t bme688_read_calib_param(bme688_calib_param_t *out);
//static uint8_t bme688_configure(void);
static uint8_t calc_res_heat(uint16_t temp);
static uint8_t bme688_is_data_ready(uint8_t profile_index, uint8_t *profile_read);

static uint8_t bme688_mem_page_set(mem_page page) {

	uint8_t adr = 0x73; // address of status register
	uint8_t mask = 1u << 4; // placement of page bit in status register

	return spi2_reg_append(adr, (uint8_t) page << 4, DEVICE_SPI_BME688, mask);
}

uint8_t bme688_is_spi(void) {
	bme688_mem_page_set(PAGE_1);

	uint8_t byte;
	byte = spi2_read_byte(SPI_BMI688_ADR_VARIANT_ID, DEVICE_SPI_BME688);
	return (byte == 0x01) ? byte : 0;
}

void bme688_init(void) {
	bme688_mem_page_set(PAGE_1);
	uint8_t reset_adr = 0x60;
	uint8_t reset_byte = 0xB6;

	spi2_send_byte(reset_adr, reset_byte, DEVICE_SPI_BME688);
	vTaskDelay(20 / portTICK_PERIOD_MS);

	bme688_read_calib_param(&calib_param_now);

	bme688_mem_page_set(PAGE_0);
	calib_param_now.res_heat_range = (spi2_read_byte(0x02, DEVICE_SPI_BME688) >> 4) & 0x03;
	calib_param_now.res_heat_val = spi2_read_byte(0x00, DEVICE_SPI_BME688);
	printf("res_heat_range: %u\n", calib_param_now.res_heat_range);
	printf("res_heat_val: %i\n", calib_param_now.res_heat_val);

	if(!bme688_configure_parallel()) {
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

float bme688_gas_res_now(uint8_t profile) {
	return  status_now.gas_res_now[profile];
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

uint8_t gas_meas = 0;
uint8_t field_now = 0;
uint8_t last_sub_meas_index = 0;
void bme688_routine(void) {
	if(!bme688_mem_page_set(PAGE_0)) {
		memset(&status_now, 0, sizeof(bme688_status_t));
		printf("bme_read_error\n");
		return;
	}

	bme688_status_t new_sample = status_now;
	new_sample.init = 1;

	// Read calibration parameters
	bme688_calib_param_t calib_param = calib_param_now;

	uint8_t profile_read = 0;

	// timeout after ~10sec
	uint16_t timeout = 1000;

	uint8_t sub_meas_index = 0;
	// here goes data check
	while(!bme688_is_data_ready(field_now, &profile_read, &sub_meas_index) && timeout) {
		timeout--;
		vTaskDelay(10 / portTICK_PERIOD_MS);
	}

	printf("profile: %u, sub_index: %u\n", profile_read, sub_meas_index);

	if(last_sub_meas_index == sub_meas_index) {
		return;
	}

	if(!timeout) {
		printf("bme688 data read fail\n");
	}

	uint8_t field_adr_incr = field_now * 0x11;
	field_now++;
	field_now %= 3;



	// Read adc values
	uint8_t adc_p_msb = spi2_read_byte(0x1f + field_adr_incr, DEVICE_SPI_BME688);
	uint8_t adc_p_lsb = spi2_read_byte(0x20 + field_adr_incr, DEVICE_SPI_BME688);
	uint8_t adc_p_xlsb = spi2_read_byte(0x21 + field_adr_incr, DEVICE_SPI_BME688);
	uint8_t adc_t_msb = spi2_read_byte(0x22 + field_adr_incr, DEVICE_SPI_BME688);
	uint8_t adc_t_lsb = spi2_read_byte(0x23 + field_adr_incr, DEVICE_SPI_BME688);
	uint8_t adc_t_xlsb = spi2_read_byte(0x24 + field_adr_incr, DEVICE_SPI_BME688);
	uint8_t adc_h_msb = spi2_read_byte(0x25 + field_adr_incr, DEVICE_SPI_BME688);
	uint8_t adc_h_lsb = spi2_read_byte(0x26 + field_adr_incr, DEVICE_SPI_BME688);
	uint8_t adc_g_lsb = spi2_read_byte(0x2d + field_adr_incr, DEVICE_SPI_BME688);
	uint8_t adc_g_msb = spi2_read_byte(0x2c + field_adr_incr, DEVICE_SPI_BME688);


	// Calculate temp
	if(profile_read == 4) {
			uint32_t temp_adc = 0;
			temp_adc = PARSE_UINT32_20BIT(adc_t_xlsb,
					adc_t_lsb, adc_t_msb);
	    int64_t var1;
	    int64_t var2;
	    int64_t var3;
	    int16_t calc_temp;

	    var1 = ((int32_t)temp_adc >> 3) - ((int32_t)calib_param.par_t1 << 1);
	    var2 = (var1 * (int32_t)calib_param.par_t2) >> 11;
	    var3 = ((((var1 >> 1) * (var1 >> 1)) >> 12) * ((int32_t)calib_param.par_t3 << 4)) >> 14;
	    calib_param.t_fine = var2 + var3;
	    calc_temp = ((calib_param.t_fine * 5) + 128) >> 8;
	    new_sample.temp_now = (((float) calc_temp) / 100);
	}

	//Calculate Humidity
	if(profile_read == 4) {
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
	if(profile_read == 4) {

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

	    /* multiplying 10000 then dividing then multiplying by 100 instead of multiplying by 1000000 to prevent overflow */
			uint32_t calc_gas_res = (10000u * var1) / (uint32_t)var2;
			calc_gas_res = calc_gas_res * 100;

			new_sample.gas_res_now[profile_read] = (float) calc_gas_res;
	}

	// update all data on successful gas data read
	status_now = new_sample;
}

static uint8_t bme688_is_data_ready(uint8_t profile_index, uint8_t *profile_read, uint8_t *sub_meas_index) {
	uint8_t index_meas_status = 0x1d + 0x11 * profile_index;
	uint8_t index_sub_meas_index = 0x1e + 0x11 * profile_index;
	uint8_t meas_status = spi2_read_byte(index_meas_status, DEVICE_SPI_BME688);
	uint8_t sub_meas_index_now = spi2_read_byte(index_sub_meas_index, DEVICE_SPI_BME688);
	uint8_t meas_status_new_data = meas_status >> 7;
	uint8_t profile_out = meas_status & 0x0F;
	if(meas_status_new_data) {
		*profile_read = profile_out;
		*sub_meas_index = sub_meas_index_now;
		return 1;
	}
	return 0;
}

static uint8_t bme688_read_calib_param(bme688_calib_param_t *out) {
	if(!bme688_mem_page_set(PAGE_1)) {
		return 0;
	}
	bme688_calib_param_t new_calib;

	//Temperature params
	uint8_t par_t2_lsb = spi2_read_byte(0x8A, DEVICE_SPI_BME688);
	uint8_t par_t2_msb = spi2_read_byte(0x8B, DEVICE_SPI_BME688);
	new_calib.par_t2 = PARSE_UINT16(par_t2_lsb, par_t2_msb);
	new_calib.par_t3 = spi2_read_byte(0x8C, DEVICE_SPI_BME688);
	uint8_t par_t1_lsb = spi2_read_byte(0xe9, DEVICE_SPI_BME688);
	uint8_t par_t1_msb = spi2_read_byte(0xea, DEVICE_SPI_BME688);
	new_calib.par_t1 = PARSE_UINT16(par_t1_lsb, par_t1_msb);

	//Humidity params
	uint8_t par_msb = spi2_read_byte(0xe3, DEVICE_SPI_BME688);
	uint8_t par_lsb = spi2_read_byte(0xe2, DEVICE_SPI_BME688) & 0x0f;
	new_calib.par_h1 = PARSE_UINT16_10B_MSBSHIFT(par_lsb, par_msb);
	par_msb = spi2_read_byte(0xe1, DEVICE_SPI_BME688);
	par_lsb = spi2_read_byte(0xe2, DEVICE_SPI_BME688) & 0xf0;
	new_calib.par_h2 = PARSE_UINT16(par_lsb, par_msb) >> 4;
	new_calib.par_h3 = spi2_read_byte(0xe4, DEVICE_SPI_BME688);
	new_calib.par_h4 = spi2_read_byte(0xe5, DEVICE_SPI_BME688);
	new_calib.par_h5 = spi2_read_byte(0xe6, DEVICE_SPI_BME688);
	new_calib.par_h6 = spi2_read_byte(0xe7, DEVICE_SPI_BME688);
	new_calib.par_h7 = spi2_read_byte(0xe8, DEVICE_SPI_BME688);

	//Pressure params
	par_msb = spi2_read_byte(0x8f, DEVICE_SPI_BME688);
	par_lsb = spi2_read_byte(0x8e, DEVICE_SPI_BME688);
	new_calib.par_p1 = PARSE_UINT16(par_lsb, par_msb);
	par_msb = spi2_read_byte(0x91, DEVICE_SPI_BME688);
	par_lsb = spi2_read_byte(0x90, DEVICE_SPI_BME688);
	new_calib.par_p2 = PARSE_UINT16(par_lsb, par_msb);
	new_calib.par_p3 = spi2_read_byte(0x92, DEVICE_SPI_BME688);
	par_msb = spi2_read_byte(0x95, DEVICE_SPI_BME688);
	par_lsb = spi2_read_byte(0x94, DEVICE_SPI_BME688);
	new_calib.par_p4 = PARSE_UINT16(par_lsb, par_msb);
	par_msb = spi2_read_byte(0x97, DEVICE_SPI_BME688);
	par_lsb = spi2_read_byte(0x96, DEVICE_SPI_BME688);
	new_calib.par_p5 = PARSE_UINT16(par_lsb, par_msb);
	new_calib.par_p6 = spi2_read_byte(0x99, DEVICE_SPI_BME688);
	new_calib.par_p7 = spi2_read_byte(0x98, DEVICE_SPI_BME688);
	par_msb = spi2_read_byte(0x9d, DEVICE_SPI_BME688);
	par_lsb = spi2_read_byte(0x9c, DEVICE_SPI_BME688);
	new_calib.par_p8 = PARSE_UINT16(par_lsb, par_msb);
	par_msb = spi2_read_byte(0x9f, DEVICE_SPI_BME688);
	par_lsb = spi2_read_byte(0x9e, DEVICE_SPI_BME688);
	new_calib.par_p9 = PARSE_UINT16(par_lsb, par_msb);
	new_calib.par_p10 = spi2_read_byte(0xa0, DEVICE_SPI_BME688);

	//Heater params
	new_calib.par_gh1 = spi2_read_byte(0xED, DEVICE_SPI_BME688);
	par_lsb = spi2_read_byte(0xEB, DEVICE_SPI_BME688);
	par_msb = spi2_read_byte(0xEC, DEVICE_SPI_BME688);
	new_calib.par_gh3 = spi2_read_byte(0xEE, DEVICE_SPI_BME688);
	new_calib.par_gh2 = PARSE_UINT16(par_lsb, par_msb);

	*out = new_calib;
	return 1;
}

static uint8_t calc_res_heat(uint16_t temp)
{

	int64_t var1, var2, var3, var4, var5, res_heat_x100, amb_temp = 25;

	var1 = (((int32_t)amb_temp * calib_param_now.par_gh3) / 10) << 8;
	var2 = (calib_param_now.par_gh1 + 784) * (((((calib_param_now.par_gh2 + 154009) * temp * 5) / 100) + 3276800) / 10);
	var3 = var1 + (var2 >> 1);
	var4 = (var3 / (calib_param_now.res_heat_range + 4));
	var5 = (131 * calib_param_now.res_heat_val) + 65536;
	res_heat_x100 = (int32_t)(((var4 / var5) - 250) * 34);
	return (uint8_t)((res_heat_x100 + 50) / 100);
}

/*
static uint8_t bme688_configure_forced(void) {

	if(!bme688_mem_page_set(PAGE_0)) {
		return 0;
	}

	uint8_t spi_ret = spi2_reg_append(SPI_BMI688_ADR_CTRL_HUM,
			SPI_BMI688_DAT_CTRL_HUM, DEVICE_SPI_BME688, SPI_BMI688_ADR_CTRL_HUM_MASK);

	spi_ret &= spi2_reg_append(SPI_BMI688_ADR_CTRL_MEAS,
			SPI_BMI688_DAT_CTRL_MEAS, DEVICE_SPI_BME688, SPI_BMI688_ADR_CTRL_MEAS_MASK);

	spi_ret &= spi2_reg_append(SPI_BMI688_ADR_CONFIG,
			SPI_BMI688_DAT_FILTER, DEVICE_SPI_BME688, SPI_BMI688_ADR_FILTER_MASK);

	spi_ret &= spi2_reg_write(SPI_BMI688_ADR_GAS_WAIT,
			SPI_BMI688_DAT_GAS_WAIT, DEVICE_SPI_BME688);

	spi_ret &= spi2_reg_write(SPI_BMI688_ADR_RES_HEAT,
			calc_res_heat(350), DEVICE_SPI_BME688);

	spi_ret &= spi2_reg_append(SPI_BMI688_ADR_CTRL_GAS_1,
			SPI_BMI688_DAT_CTRL_GAS_1_NB0, DEVICE_SPI_BME688, SPI_BMI688_ADR_CTRL_GAS_1_MASK_NBCONV);

	spi_ret &= spi2_reg_append(SPI_BMI688_ADR_CTRL_GAS_0,
			SPI_BMI688_DAT_CTRL_GAS_0_RUN_GAS, DEVICE_SPI_BME688, SPI_BMI688_ADR_CTRL_GAS_0_MASK_HEATOFF);

	spi_ret &= spi2_reg_append(SPI_BMI688_ADR_CTRL_GAS_1,
			SPI_BMI688_DAT_CTRL_GAS_1_RUN_GAS, DEVICE_SPI_BME688, SPI_BMI688_ADR_CTRL_GAS_1_MASK_RUN_GAS);

	//set to forced mode
	spi_ret &= spi2_reg_write(SPI_BMI688_ADR_CTRL_MEAS,
			SPI_BMI688_DAT_CTRL_MEAS_FORCED, DEVICE_SPI_BME688);
	return spi_ret;
}
*/

static uint16_t bme688_map_heater_ht[10] = {320, 100, 100, 100, 200, 200, 200, 320, 320, 320};
static uint16_t bme688_map_heater_wait[10] = {2, 0, 3, 26, 2, 1, 1, 2, 1, 1};

static uint8_t bme688_configure_parallel(void) {
	if(!bme688_mem_page_set(PAGE_0)) {
		return 0;
	}



	//set H oversampling, 3 wire interupt off
	uint8_t res = spi2_reg_append(SPI_BMI688_ADR_CTRL_HUM,
			OSRS_H_VAL, DEVICE_SPI_BME688, 0x47);

	// set TP oversampling and sleep mode
	res &= spi2_reg_write(SPI_BMI688_ADR_CTRL_MEAS,
			OSRS_T_VAL | OSRS_P_VAL | MODE_SLEEP,
			DEVICE_SPI_BME688);

	// set Filter, off 3-wire spi
	res &= spi2_reg_append(SPI_BMI688_ADR_CONFIG,
			SPI_BMI688_DAT_FILTER, DEVICE_SPI_BME688, 0x1D);

	// enable gas and nb_conv=0
	res &= spi2_reg_append(SPI_BMI688_ADR_CTRL_GAS_1,
			SPI_BMI688_DAT_CTRL_GAS_1_RUN_GAS_PAR, DEVICE_SPI_BME688, SPI_BMI688_DAT_CTRL_GAS_1_MASK);

	// set gas wait shared
	res &= spi2_reg_write(SPI_BMI688_DAT_GAS_WAITSH,
			WAITSH_MULT | WAITSH_VAL, DEVICE_SPI_BME688);
	// set gas wait for plate 0
	for(uint8_t i = 0; i < 10; i++) {
		res &= spi2_reg_write(SPI_BMI688_ADR_GAS_WAIT + i,
				bme688_map_heater_wait[i], DEVICE_SPI_BME688);

		res &= spi2_reg_write(SPI_BMI688_ADR_RES_HEAT + i,
				calc_res_heat(bme688_map_heater_ht[i]), DEVICE_SPI_BME688);
	}

	// ensure heating in on
	res &= spi2_reg_append(SPI_BMI688_ADR_CTRL_GAS_0,
			SPI_BMI688_DAT_CTRL_GAS_0_RUN_GAS, DEVICE_SPI_BME688, SPI_BMI688_DAT_CTRL_GAS_0_MASK);

	// set parallel mode
	res &= spi2_reg_write(SPI_BMI688_ADR_CTRL_MEAS,
			OSRS_T_VAL | OSRS_P_VAL | MODE_PARALLEL,
			DEVICE_SPI_BME688);

	return res;
}
