/*
 * bme688.h
 *
 *  Created on: Apr 29, 2022
 *      Author: Jakub Tomczak
 */

#ifndef MAIN_SPI2_BME688_H_
#define MAIN_SPI2_BME688_H_

#include <stdint.h>

#define SPI_BMI688_ADR_VARIANT_ID 	0x70

#define SPI_BMI688_ADR_CTRL_MEAS				0x74
#define SPI_BMI688_ADR_CTRL_HUM					0x72
#define SPI_BMI688_ADR_CONFIG					0x75
#define SPI_BMI688_ADR_CTRL_GAS_1				0x71
#define SPI_BMI688_ADR_CTRL_GAS_0				0x70
#define SPI_BMI688_ADR_GAS_WAIT					0x64
#define SPI_BMI688_ADR_RES_HEAT					0x5A
#define SPI_BMI688_ADR_ADC_MSB					0x22

#define SPI_BMI688_DAT_CTRL_MEAS				0x54
#define SPI_BMI688_DAT_CTRL_HUM					0x01
#define SPI_BMI688_DAT_FILTER					(0b10 << 2)
#define SPI_BMI688_DAT_GAS_WAIT					0xCA
#define SPI_BMI688_DAT_GAS_WAITSH				0x6e
#define SPI_BMI688_DAT_CTRL_GAS_1_RUN_GAS		0x20
#define SPI_BMI688_DAT_CTRL_GAS_1_RUN_GAS_PAR	0x2A
#define SPI_BMI688_DAT_CTRL_GAS_1_MASK			0x2f
#define SPI_BMI688_DAT_CTRL_GAS_0_RUN_GAS		0
#define SPI_BMI688_DAT_CTRL_GAS_0_MASK			0x08
#define SPI_BMI688_DAT_CTRL_GAS_1_NB0			0
#define SPI_BMI688_DAT_CTRL_MEAS_FORCED			0x55

#define OSRS_T_VAL				(0b010 << 5)
#define OSRS_P_VAL				(0b010 << 2)
#define OSRS_H_VAL				0b010
#define MODE_SLEEP				0x00
#define MODE_PARALLEL			0x02
#define MODE_FORCED				0x01
#define WAITSH_MULT				(0x01 << 6)
#define WAITSH_VAL				52u
#define GAS_WAIT				0

#define SPI_BMI688_ADR_CTRL_MEAS_MASK			0xfc
#define SPI_BMI688_ADR_CTRL_HUM_MASK 			0b00000111
#define SPI_BMI688_ADR_FILTER_MASK				0b00011100
#define SPI_BMI688_ADR_CTRL_GAS_1_MASK_RUN_GAS 	(2u << 4)
#define SPI_BMI688_ADR_CTRL_GAS_1_MASK_NBCONV 	0b00001111
#define SPI_BMI688_ADR_CTRL_GAS_0_MASK_HEATOFF 	0b00001000


uint8_t bme688_is_spi(void);
void bme688_init(void);
void bme688_routine(void);

float bme688_celcius_now(void);
float bme688_prH_now(void);
float bme688_pressure_now(void);
float bme688_gas_res_now(uint8_t profile);
void bme688_gas_res_print_all(void);
uint8_t bme688_is_init(void);

#endif /* MAIN_SPI2_BME688_H_ */
