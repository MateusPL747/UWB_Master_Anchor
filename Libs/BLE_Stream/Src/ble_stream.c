#include "main.h"
#include "ble_stream.h"
#include "custom-mqtt.h"
#include <stdint.h>
#include <string.h>

#define BLE_UART_BUFFER_SIZE 33
static uint8_t ble_buffer[BLE_UART_BUFFER_SIZE];
uint16_t received;
extern mqt_t mqtt_handler;     // Defined here but declared at file ../../custom-mqtt.c

uint8_t rx_pending = 0;

/**
 * @brief Starts the BLE mesh data reception using the UART bus
 * 
 * @param huart 
 */
void ble_sniff_init ( UART_HandleTypeDef *huart ) {

    // Start receiving UART data from ble mesh
    HAL_UARTEx_ReceiveToIdle_IT(
        huart,
        ble_buffer,
        BLE_UART_BUFFER_SIZE
    );

}

/**
 * @brief This function serves as a uart callback specifically for the ble callbacks. When the
 * HAL_UARTEx_RxEventCallback is called by the hardware, it should redirect to this function if
 * huart->Instance = ble_huart
 * 
 * @param Size 
 */
void ble_uart_callback ( UART_HandleTypeDef *huart, uint16_t Size ) {
    rx_pending = 1;
    received = Size;

    // Re-start receiving UART data from ble mesh
    HAL_UARTEx_ReceiveToIdle_IT(
        huart,
        ble_buffer,
        BLE_UART_BUFFER_SIZE
    );
}

/**
 * @brief Process the incomming BLE mesh
 * 
 */
void ble_task () {
    // Do something with the BLE data. Eg. Send it to mqtt server
    if ( rx_pending && !mqtt_handler.pending_mqtt_connection && !mqtt_handler.requestForSent ) {
        mqtt_handler.msgLen = received;
        memcpy( mqtt_handler.msgBuff, ble_buffer,  mqtt_handler.msgLen);

        mqtt_handler.requestForSent = 1;
        rx_pending = 0;
    }
}