#include "custom-at.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

extern ATmssg_t stateMachine; // Defined in main.c

void setExpected(char *newExpected)
{
    stateMachine.resume_msg = newExpected;
}

void setRejectExcepcted(char *newReject)
{
    stateMachine.reject_msg = newReject;
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

    stateMachine.huartAT = huart;

    stateMachine.apn_host = apn_host;
    stateMachine.apn_user = apn_user;
    stateMachine.apn_psw = apn_psw;

    stateMachine.mqtt_host = mqtt_host;
    stateMachine.mqtt_port = mqtt_port;

    stateMachine.error_count = 0;
    stateMachine.interruptFlag = 0;
    stateMachine.clearAllFlag = 0;

    // Disable the at command echo back and restart the moden. Then directly change the state to WAITIN_SIM
    HAL_UART_Transmit_IT(stateMachine.huartAT, (unsigned char *)"ATE0&W\r\nAT+CFUN=1,1\r\n", 21);
    set_waiting_sim_state();

    // Start listening for modem UART
    HAL_UARTEx_ReceiveToIdle_IT(
        stateMachine.huartAT,
        (unsigned char *) stateMachine.incomingMsg,
        sizeof( stateMachine.incomingMsg )
    );
}

void sendAT(char *msg, size_t sizetoSend )
{
    if ( sizetoSend == (size_t)NULL ) {
        HAL_UART_Transmit_IT(stateMachine.huartAT, (const unsigned char *) msg, strlen(msg));
    } else {
        HAL_UART_Transmit_IT(stateMachine.huartAT, (const unsigned char *) msg, sizetoSend);
    }
}

void GSM_task()
{

    resolveUARTCtrl( stateMachine.huartAT );
    clearUARTCtrl( stateMachine.huartAT );

    /* Only start the task after 2s */
    if ( HAL_GetTick() < 2000 )
        return;

    /* If already connected, stop sending automatic AT commands. Now, only the application should send them */
    if ( stateMachine.state > TCP_CONNECTING  )
        return;
    
    /* If it is in TCP_CLOSED state, direct change to IP_STATUS */
    if ( stateMachine.state == TCP_CLOSED )
        set_ip_status_state();
    
    /* If it is in ERROR_STATE state */
    else if ( stateMachine.state == ERROR_STATE )
    {
        HAL_UART_Transmit_IT(stateMachine.huartAT, (unsigned char *)"ATE0&W\r\nAT+CFUN=1,1\r\n", 21);
        set_waiting_sim_state();
    }

    /* If doesn't reach this state timeout, don't send commands */
    if ( ( HAL_GetTick() - stateMachine.lastChangestateTime ) < stateMachine.timeout )
        return;

    stateMachine.lastChangestateTime = HAL_GetTick();
    sendAT( stateMachine.command, (size_t)NULL );

}