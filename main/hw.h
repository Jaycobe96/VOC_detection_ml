/*
 * hw.h
 *
 *  Created on: Apr 29, 2022
 *      Author: Jakub Tomczak
 */

#ifndef MAIN_HW_H_
#define MAIN_HW_H_

#define HW_I2C_BME688_SDA	4
#define HW_I2C_BME688_SCL	5
#define HW_PWM_LED			48

#include <stdint.h>
#include "driver/spi_master.h"

void spi2_init(void);
void i2s_init(void);
void uart2_init(void);

spi_device_handle_t* hw_get_spi2_device_handle(uint8_t device_num);



#endif /* MAIN_HW_H_ */
