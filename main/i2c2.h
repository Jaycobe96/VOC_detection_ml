/*
 * i2c.h
 *
 *  Created on: 23 Sep 2022
 *      Author: s174873
 */

#ifndef MAIN_I2C2_H_
#define MAIN_I2C2_H_

#include <stdint.h>
#include <stdbool.h>

void i2c2_init(void);

uint8_t i2c2_read_byte(uint8_t reg, uint8_t adr);
uint8_t i2c2_send_byte(uint8_t reg, uint8_t data, uint8_t adr);

uint8_t i2c2_reg_write(uint8_t reg, uint8_t data, uint8_t adr);
uint8_t i2c2_reg_append(uint8_t reg, uint8_t data, uint8_t adr, uint8_t mask);
uint8_t i2c2_reg_append_wait(uint8_t reg, uint8_t data, uint8_t adr, uint8_t mask, uint8_t ms_wait);



#endif /* MAIN_I2C2_H_ */
