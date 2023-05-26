/*
 * spi2.h
 *
 *  Created on: Jun 2, 2022
 *      Author: Jakub Tomczak
 */

#ifndef MAIN_SPI2_H_
#define MAIN_SPI2_H_

#include "hw.h"

#include <stdint.h>

#define PARSE_UINT16(LSB, MSB) (((uint16_t)MSB << 8) | (uint16_t)LSB)
#define PARSE_UINT32_20BIT(LSB, MID, MSB) (((uint32_t)MSB << 12) | ((uint32_t)MID << 4) | (uint32_t)LSB >> 4)
#define PARSE_UINT16_10BIT(LSB, MSB) (((uint16_t)MSB << 2) | (uint16_t)LSB >> 6)
#define PARSE_UINT16_10B_MSBSHIFT(LSB, MSB) (((uint16_t)MSB << 4) | (uint16_t)LSB)

typedef enum {
	DEVICE_SPI_LIS3DH = 0,
	DEVICE_SPI_BME688,
}spi2_device;

uint8_t spi2_read_byte(uint8_t adr, spi2_device device);
void spi2_read_byte_burst(uint8_t sta_adr, spi2_device device,size_t len, uint8_t *buff);
uint8_t spi2_send_byte(uint8_t adr, uint8_t data, spi2_device device);
uint8_t spi2_read_byte_norbit(uint8_t adr, spi2_device device);

uint8_t spi2_reg_write(uint8_t adr,uint8_t data, spi2_device device);
uint8_t spi2_reg_append(uint8_t adr, uint8_t data, spi2_device device, uint8_t mask);
uint8_t spi2_reg_append_wait(uint8_t adr, uint8_t data, spi2_device device, uint8_t mask, uint8_t ms_wait);
#endif /* MAIN_SPI2_H_ */
