#include "custom-at.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

ATmssg_t GSMstateMachine;

void setExpected(char *newExpected)
{
    GSMstateMachine.resume_msg = newExpected;
}

void setRejectExpected(char *newReject)
{
    GSMstateMachine.reject_msg = newReject;
}

void AT_usart_init_config(

    UART_HandleTypeDef *huart,
    char *apn_host,
    char *apn_user,
    char *apn_psw,
    char * mqtt_host,
    char * mqtt_user,
    char * mqtt_psw,
    int mqtt_port)
{
    GSMstateMachine.state = IDLE_DISCONNECTED;
    setExpected((char *)"\r\nOK\r\n\r\nSTATE: IP INITIAL\r\n");
    setRejectExpected((char *)"\r\nERROR\r\n");
    sprintf( GSMstateMachine.command, "AT\r\nWAIT=1\r\nAT+CIPSTATUS\r\n" );

    GSMstateMachine.lastChangestateTime = HAL_GetTick();
    GSMstateMachine.timeout = 5 * STATE_TIMEOUT;

    GSMstateMachine.huartAT = huart;

    GSMstateMachine.apn_host = apn_host;
    GSMstateMachine.apn_user = apn_user;
    GSMstateMachine.apn_psw = apn_psw;

    GSMstateMachine.mqtt_host = mqtt_host;
    GSMstateMachine.mqtt_port = mqtt_port;

    GSMstateMachine.error_count = 0;
    GSMstateMachine.interruptFlag = 0;
    GSMstateMachine.clearAllFlag = 0;

    // Disable the at command echo back and restart the moden
    HAL_UART_Transmit_IT(GSMstateMachine.huartAT, (unsigned char *)"ATE0&W\r\nAT+CFUN=1,1\r\n", 21);

    // Start receiving UART data from modem
    HAL_UARTEx_ReceiveToIdle_IT(
        GSMstateMachine.huartAT,
        (unsigned char *) GSMstateMachine.incomingMsg,
        sizeof( GSMstateMachine.incomingMsg )
    );
}

void sendAT(char *msg )
{
    HAL_UART_Transmit_IT(GSMstateMachine.huartAT, (const unsigned char *) msg, strlen(msg));
}

void GSM_task(){

    resolveUARTCtrl( GSMstateMachine.huartAT );
    clearUARTCtrl( GSMstateMachine.huartAT );

    /* Only start the task after 2s */
    if ( HAL_GetTick() < 2000 )
        return;

    /* If already connected, stop sending automatic AT commands. Now, only the application should send them */
    if ( GSMstateMachine.state > TCP_CONNECTING )
        return;

    /* If doesn't reach this state timeout, don't send commands */
    if ( ( HAL_GetTick() - GSMstateMachine.lastChangestateTime ) < GSMstateMachine.timeout )
        return;

    GSMstateMachine.lastChangestateTime = HAL_GetTick();
    sendAT( GSMstateMachine.command );

}