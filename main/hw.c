/*
 * hw.c
 *
 *  Created on: Apr 29, 2022
 *      Author: Jakub Tomczak
 */

#include "hw.h"

#include "driver/spi_common.h"
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "driver/i2s.h"
#include "driver/uart.h"

#define HW_SPI2_MISO		15
#define HW_SPI2_MOSI		16
#define HW_SPI2_CLK			17
#define HW_SPI2_LIS3DH_CS	18
#define HW_SPI2_BME688_CS	9

#define HW_I2S_WSEL			3
#define HW_I2S_DATIN		4

#define HW_UART2_TX			11
#define HW_UART2_RX			12

const uart_port_t uart_num = UART_NUM_2;
QueueHandle_t uart_queue;
const int uart_buffer_size = (1024 * 2);

uart_config_t uart_config = {
    .baud_rate = 115200,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .rx_flow_ctrl_thresh = 122,
};

static const spi_bus_config_t hw_cfg_spi_bus = {
		.mosi_io_num = HW_SPI2_MOSI,
		.miso_io_num = HW_SPI2_MISO,
		.sclk_io_num = HW_SPI2_CLK,

		.data2_io_num = -1,
		.data3_io_num = -1,
		.data4_io_num = -1,
		.data5_io_num = -1,
		.data6_io_num = -1,
		.data7_io_num = -1,

		.max_transfer_sz = 10,
		.flags = SPICOMMON_BUSFLAG_MASTER,
		.intr_flags = 0
};

static spi_device_handle_t hw_spi2_lis3dh_handle = {0};
static spi_device_handle_t hw_spi2_bme688_handle = {0};

// same config works for bme688 and lis3dh
static spi_device_interface_config_t spi_lis3dh_interface = {
		.command_bits = 0,
		.address_bits = 8,
		.dummy_bits = 0,
		.mode = 0, // CPOL = 1, CPHA = 1
		.duty_cycle_pos = 0,
		.cs_ena_pretrans = 0,
		.cs_ena_posttrans = 0,
		.clock_speed_hz = SPI_MASTER_FREQ_8M,
		.input_delay_ns = 50,
		.spics_io_num = HW_SPI2_LIS3DH_CS, // lis3dh as default
		.flags = SPI_DEVICE_NO_DUMMY,
		.queue_size = 4, // transaction buffer size
		.pre_cb = NULL,
		.post_cb = NULL
};

static spi_device_interface_config_t spi_bme688_interface = {
		.command_bits = 0,
		.address_bits = 8,
		.dummy_bits = 0,
		.mode = 0, // CPOL = 0, CPHA = 0
		.duty_cycle_pos = 0,
		.cs_ena_pretrans = 0,
		.cs_ena_posttrans = 0,
		.clock_speed_hz = SPI_MASTER_FREQ_8M,
		.input_delay_ns = 20,
		.spics_io_num = HW_SPI2_BME688_CS,
		.flags = SPI_DEVICE_NO_DUMMY,
		.queue_size = 4, // transaction buffer size
		.pre_cb = NULL,
		.post_cb = NULL
};

i2s_config_t i2s_config = {
		.mode = I2S_MODE_MASTER |I2S_MODE_RX | I2S_MODE_PDM,
		.sample_rate = 15625,
		.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
		.channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
		.communication_format = I2S_COMM_FORMAT_STAND_I2S,
		 .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2,
		.dma_buf_count = 16,
		.dma_buf_len = 64,
		.use_apll = 0,
};

static const i2s_pin_config_t i2s_pin_config = {
		.ws_io_num = HW_I2S_WSEL,
		.data_in_num = HW_I2S_DATIN,
		.data_out_num = I2S_PIN_NO_CHANGE
};

void spi2_init(void) {
	ESP_ERROR_CHECK_WITHOUT_ABORT(spi_bus_initialize(SPI2_HOST, &hw_cfg_spi_bus, SPI_DMA_DISABLED));
	ESP_ERROR_CHECK_WITHOUT_ABORT(spi_bus_add_device(SPI2_HOST, &spi_lis3dh_interface, &hw_spi2_lis3dh_handle));
	ESP_ERROR_CHECK_WITHOUT_ABORT(spi_bus_add_device(SPI2_HOST, &spi_bme688_interface, &hw_spi2_bme688_handle));
}

void i2s_init(void) {
	ESP_ERROR_CHECK_WITHOUT_ABORT(i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL));
	ESP_ERROR_CHECK_WITHOUT_ABORT(i2s_set_pin(I2S_NUM_0, &i2s_pin_config));
	ESP_ERROR_CHECK_WITHOUT_ABORT(i2s_set_pdm_rx_down_sample(I2S_NUM_0, I2S_PDM_DSR_16S));
}

void uart2_init(void) {
	ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));
	ESP_ERROR_CHECK(uart_set_pin(UART_NUM_2, HW_UART2_TX, HW_UART2_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
	// Install UART driver using an event queue here
	ESP_ERROR_CHECK(uart_driver_install(UART_NUM_2, uart_buffer_size, \
	                                        uart_buffer_size, 10, &uart_queue, 0));
}

spi_device_handle_t* hw_get_spi2_device_handle(uint8_t device_num) {

	// Expand for spi2 new device
	if(device_num == 1) {
		return &hw_spi2_lis3dh_handle;
	} else if(device_num == 2) {
		return &hw_spi2_bme688_handle;
	}
	return NULL;
}


