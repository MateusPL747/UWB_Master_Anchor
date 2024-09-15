#ifndef BLE_STREAM_H
#define BLE_STREAM_H

void ble_uart_callback ( UART_HandleTypeDef *huart, uint16_t Size );

void ble_sniff_init ( UART_HandleTypeDef *huart );

void ble_task ();

#endif