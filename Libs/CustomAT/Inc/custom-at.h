#ifndef CUSTOM_AT_H
#define CUSTOM_AT_H

#include "stm32f1xx.h"
#include <stdint.h>
#include <stdio.h>

#define WAITING_SIM         0x00
#define IDLE_DISCONNECTED   0x01
#define IP_INITIAL          0x02
#define IP_START            0x03
#define IP_GPRSACT          0x04
#define IP_STATUS           0x05

#define TCP_CONNECTING      0x06
#define CONNECT_OK          0x07
#define TCP_CLOSED          0x08
#define ERROR_STATE         0x09

#define CIICR_TIMEOUT       85000UL
#define CIPSTART_TIMEOUT    75000UL
#define STATE_TIMEOUT       1000UL

#define ERR_COUNT_OUT  3

#define INFINITE_TIMEOUT    0xFFFFFFUL

typedef struct
{
    UART_HandleTypeDef *huartAT;
    int8_t state;
    int32_t timeout;
    char *resume_msg;
    char *reject_msg;
    char command[61];
    int32_t lastChangestateTime;

    char *apn_host;
    char *apn_user;
    char *apn_psw;

    char *mqtt_host;
    int mqtt_port;

    uint16_t incomingMsgSize;
    char incomingMsg[30];
    int error_count;

    int interruptFlag;
    int clearAllFlag;

} ATmssg_t;

void setExpected(char *newExpected);

void setRejectExcepcted(char *newReject);

void AT_usart_init_config(UART_HandleTypeDef *huart, char *apn_host, char *apn_user, char *apn_psw, char * mqtt_host, char * mqtt_user, char * mqtt_psw, int mqtt_port);

void sendAT( char *msg, size_t sizetoSend );

void GSM_task();

void set_ip_initial_state();

void set_ip_start_state();

void set_ip_gprsact_state();

void set_ip_status_state();

void set_tcp_connecting_state();

void set_connect_ok_state();

void set_tcp_closed_state ();

void set_error_state ();

int is_error_count_out ();

void resolveUARTCtrl ( UART_HandleTypeDef *huart );

void clearUARTCtrl ( UART_HandleTypeDef *huart );

#endif