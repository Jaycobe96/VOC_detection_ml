/*
 * bluetooh_wrapper.h
 *
 *  Created on: Jun 2, 2022
 *      Author: Jakub Tomczak
 */

#ifndef MAIN_BLUETOOTH_WRAPPER_H_
#define MAIN_BLUETOOTH_WRAPPER_H_

#include <stdio.h>
#include <stdint.h>

void bt_init(void);
void bt_gatt_send(uint8_t *buff, size_t size);

uint8_t** bt_gatt_buffer_init(size_t size);
void bt_gatt_buffer_set(uint8_t *buff_in, uint8_t** buff_out, size_t size);
size_t bt_gatt_size_to_pages_count(size_t size);
void bt_gatt_send_page(uint8_t** buff, size_t page);

void bt_indicate_data(uint32_t* new_sample);
void bt_get_out_characteristic(uint8_t* buff_out);
void bt_clear_out_characteristic();


#endif /* MAIN_BLUETOOTH_WRAPPER_H_ */
