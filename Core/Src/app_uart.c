#include "main.h"
#include "custom-at.h"
#include "ble_stream.h"

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
    if ( huart->Instance == USART1 ) {
        gsm_uart_callback ( huart, Size );
    } else if ( huart->Instance == USART3 ){
        ble_uart_callback( huart, Size );
    }
}