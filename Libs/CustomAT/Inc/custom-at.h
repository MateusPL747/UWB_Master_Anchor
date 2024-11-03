/*
States:
TO_CONFG_APN
    command: AT+CSTT="<apn>","<apn_usr>","<apn_pwd>"
    
    resume: OK
    nextStep: TO_CONFG_GSM
    
    reject: ERROR
    
    
    nextStep: reset_modemSW

TO_CONFG_GSM
    command: AT+CIICR

    resume: OK
    nextStep: TO_CONG_IP

    reject: ERROR
    nextStep: TO_CONFG_APN

    timeout: 85000ms

TO_CONG_IP
    command: AT+AT+CIFSR

    resume: OK
    nextStep: TO_CONFG_TCP

    reject: ERROR
    nextStep: TO_CONFG_GSM


TO_CONFG_TCP
    command: AT+CIPSTART="TCP","<broker_add>",<port_number>

    resume: OK
    nextStep: IDLE

    reject: ERROR
    nextStep: TO_CONG_IP

    timeout: 160000 ms

TRASNMITING
    command:
        AT+CIPSEND
        <HEX>

    

RESET_MODEM_SW
    command: AT+CFUN=1,1

    resume: OK
    nextStep: TO_CONFG_APN

    reject: +CME ERROR: <err>
    nextStep: RESET_MODEM_HW

    timeout: 10000ms

RESET_MODEM_HW
    command: 'Pulls down the power pin'

IDLE_CONNECTED:
    loop: healthCheck

IDLE_DISCONNECTED:
    loop: checkReady
*/

#ifndef CUSTOM_AT_H
#define CUSTOM_AT_H

#include "stm32f1xx.h"
#include <stdint.h>
#include <stdio.h>

#define WAITING_SIM         0x01
#define IDLE_DISCONNECTED   0x02
#define IP_INITIAL          0x03
#define IP_START            0x04
#define IP_GPRSACT          0x05
#define IP_STATUS           0x06
#define TCP_CONNECTING      0x07
#define CONNECT_OK          0x08
#define TCP_CLOSED          0x09
#define SENDING_MSG         0x10
#define INITIAL_STATE       0x00
#define ERROR_STATE         0x00
#define UNDEFINED_STATE     0xEE

#define CIICR_TIMEOUT       10000UL//85000UL//
#define CIPSTART_TIMEOUT    10000UL//75000UL//
#define STATE_TIMEOUT       1000UL
#define CFUN_TIME           10000UL//5000UL//
#define CPIN_TIME           5000UL

#define ERR_COUNT_OUT  3

#define INFINITE_TIMEOUT    0xFFFFFFUL

#define CONNECTION_CHECK_INTERVAL 5000UL
#define MODEM_CHECK_INTERVAL 85000UL//5000UL//    // Needs to be equal than the higher timeout (CIICR_TIMEOUT)

typedef struct
{
    UART_HandleTypeDef *huartAT;
    int8_t state;
    int32_t timeout;
    int8_t disable_state_timeout;
    int32_t time_to_check_connection;
    int32_t time_to_check_modem;
    int8_t modem_is_alive;
    int32_t err_countout;
    char *resume_msg;
    char *reject_msg;
    char command[61];
    int8_t command_len;
    int32_t lastChangestateTime;
    int pin;

    char *apn_host;
    char *apn_user;
    char *apn_psw;

    char *mqtt_host;
    int mqtt_port;

    uint16_t incomingMsgSize;
    char incomingMsg[33];
    int error_count;

    int interruptFlag;
    int clearAllFlag;

} ATmssg_t;

void setExpected(char *newExpected);

void setRejectExpected(char *newReject);

void AT_usart_init_config(UART_HandleTypeDef *huart, char *apn_host, char *apn_user, char *apn_psw, int pin_user, char * mqtt_host, char * mqtt_user, char * mqtt_psw, int mqtt_port);

void sendAT(char *cmd, int len);

void GSM_task();

void set_waiting_sim_state();

void set_idle_disconnected_state();

void set_ip_initial_state();

void set_ip_start_state();

void set_ip_gprsact_state();

void set_ip_status_state();

void set_tcp_connecting_state();

void set_connect_ok_state();

void set_error_state ();

void set_sending_msg_state();

int is_error_count_out ();

int is_state_timeout ();

void is_GSM_online ();

void is_GSM_alive ();

void gsm_uart_callback ( UART_HandleTypeDef * huart, uint16_t Size );

void resolveUARTCtrl ( UART_HandleTypeDef *huart );

void clearUARTCtrl ( UART_HandleTypeDef *huart );

void resetGSM ();

#endif