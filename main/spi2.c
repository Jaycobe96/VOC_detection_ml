/*
 * spi2.c
 *
 *  Created on: Jun 2, 2022
 *      Author: Jakub Tomczak
 */

#include "spi2.h"

#include "esp_system.h"
#include "esp_event.h"
#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"

static void spi2_transmission_setup(spi_transaction_t *cfg, size_t len, uint32_t flags);
static spi_device_handle_t* spi2_get_device_handle(spi2_device device);


uint8_t spi2_read_byte(uint8_t adr, spi2_device device) {
	// RW bit works same for lis3dh and bme688

	spi_transaction_t send_cfg;
	spi_device_handle_t *device_handle = spi2_get_device_handle(device);

	spi2_transmission_setup(&send_cfg, 8, SPI_TRANS_USE_RXDATA | SPI_TRANS_USE_TXDATA);

	send_cfg.tx_data[0] = 0;

	if(device == DEVICE_SPI_BME688 ||
			device == DEVICE_SPI_LIS3DH ) {
		send_cfg.addr = adr | (1 << 7);
	}

	ESP_ERROR_CHECK_WITHOUT_ABORT(spi_device_transmit(*device_handle, &send_cfg));

	//printf("rx_data:%u\n", send_cfg.rx_data[0]);

	return send_cfg.rx_data[0];
}

void spi2_read_byte_burst(uint8_t sta_adr, spi2_device device, size_t len, uint8_t *buff) {

	spi_transaction_t send_cfg;
	spi_device_handle_t *device_handle = spi2_get_device_handle(device);

	spi2_transmission_setup(&send_cfg, len, 0);

	send_cfg.rx_buffer = (void*) buff;

	if(device == DEVICE_SPI_BME688 ||
			device == DEVICE_SPI_LIS3DH) {
		send_cfg.addr = sta_adr | (1 << 7);
	}

	ESP_ERROR_CHECK_WITHOUT_ABORT(spi_device_transmit(*device_handle, &send_cfg));

	//printf("rx_data:%u\n", send_cfg.rx_data[0]);
}

uint8_t spi2_send_byte(uint8_t adr, uint8_t data, spi2_device device) {
	spi_transaction_t send_cfg;
	spi_device_handle_t *device_handle = spi2_get_device_handle(device);

	spi2_transmission_setup(&send_cfg, 8, SPI_TRANS_USE_RXDATA | SPI_TRANS_USE_TXDATA);
	send_cfg.addr = adr;
	send_cfg.tx_data[0] = data;

	ESP_ERROR_CHECK_WITHOUT_ABORT(spi_device_transmit(*device_handle, &send_cfg));

	return send_cfg.rx_data[0];
}

uint8_t spi2_reg_write(uint8_t adr,uint8_t data, spi2_device device) {
	spi2_send_byte(adr, data, device);
	return (data == spi2_read_byte(adr, device)) ? 1 : 0;
}

uint8_t spi2_reg_append(uint8_t adr, uint8_t data, spi2_device device, uint8_t mask) {
	uint8_t feedback = spi2_read_byte(adr, device);
	feedback &= ~mask;
	uint8_t set = feedback | data;
	return spi2_reg_write(adr, set, device);
}

uint8_t spi2_reg_append_wait(uint8_t adr, uint8_t data, spi2_device device, uint8_t mask, uint8_t ms_wait) {
	uint8_t feedback = spi2_read_byte(adr, device);
	vTaskDelay(ms_wait / portTICK_PERIOD_MS);
	feedback &= ~mask;
	uint8_t set = feedback | data;
	return spi2_reg_write(adr, set, device);
}

static void spi2_transmission_setup(spi_transaction_t *cfg, size_t len, uint32_t flags) {
	cfg->flags = flags;
	cfg->cmd = 0;
	cfg->length = len;
	cfg->rxlength = len;
	cfg->user = NULL;
}

static spi_device_handle_t* spi2_get_device_handle(spi2_device device) {
	switch (device) {
	case DEVICE_SPI_LIS3DH : return hw_get_spi2_device_handle(1); break;
	case DEVICE_SPI_BME688 : return hw_get_spi2_device_handle(2); break;
	}
	return NULL;
}
