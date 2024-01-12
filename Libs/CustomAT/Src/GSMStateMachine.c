#include "custom-at.h"
#include "custom-mqtt.h"
#include <string.h>

int ip_a;
int ip_b;
int ip_c;
int ip_d;
char * msgBuff;
size_t totalMsgLen;

extern ATmssg_t stateMachine; // Defined here but declared at file: ./custom-at.c
extern mqt_t mqtt_handle;     // Defined here but declared at file ../../custom-mqtt.c

void set_wating_sim_state () {
    setExpected((char *)"\r\nSMS Ready\r\n");
    stateMachine.state = WATING_SIM;
    stateMachine.timeout = STATE_TIMEOUT;
    stateMachine.lastChangestateTime = HAL_GetTick();
}

void set_ip_initial_state() {
    sprintf( stateMachine.command, "AT+CIPSTATUS\r\n" );
    setExpected((char *)"\r\nOK\r\n\r\nSTATE: IP INITIAL\r\n");
    stateMachine.state = IP_INITIAL;
    stateMachine.timeout = STATE_TIMEOUT;
    stateMachine.lastChangestateTime = HAL_GetTick();
    sprintf( stateMachine.command, "AT+CSTT=\"%s\",\"%s\",\"%s\"\r\n", stateMachine.apn_host, stateMachine.apn_user, stateMachine.apn_psw );

    /* Sends a command as soon as it change state */
    sendAT( stateMachine.command, NULL );
};

void set_ip_start_state() {
    setExpected((char *)"\r\nOK\r\n");
    stateMachine.state = IP_START;
    stateMachine.timeout = CIICR_TIMEOUT;
    stateMachine.lastChangestateTime = HAL_GetTick();
    sprintf( stateMachine.command, "AT+CIICR\r\n" );

    /* Sends a command as soon as it change state */
    sendAT( stateMachine.command, NULL );
};

void set_ip_gprsact_state() {
    setExpected((char *)"\r\nOK\r\n");
    stateMachine.state = IP_GPRSACT;
    stateMachine.timeout = STATE_TIMEOUT;
    stateMachine.lastChangestateTime = HAL_GetTick();
    sprintf( stateMachine.command, "AT+CIFSR\r\n" );

    /* Sends a command as soon as it change state */
    sendAT( stateMachine.command, NULL );
};

void set_ip_status_state() {
    setExpected((char *)"\r\nOK\r\n");
    stateMachine.state = IP_STATUS;
    stateMachine.timeout = STATE_TIMEOUT;
    stateMachine.lastChangestateTime = HAL_GetTick();
    sprintf( stateMachine.command, "AT+CIPSTART=\"TCP\",\"%s\",%d\r\n", stateMachine.mqtt_host, stateMachine.mqtt_port );

    /* Sends a command as soon as it change state */
    sendAT( stateMachine.command, NULL );
};

void set_tcp_connecting_state() {
    stateMachine.state = TCP_CONNECTING;
    stateMachine.timeout = INFINITE_TIMEOUT;
    stateMachine.lastChangestateTime = HAL_GetTick();
};

void set_connect_ok_state() {
    stateMachine.state = CONNECT_OK;
    stateMachine.timeout = INFINITE_TIMEOUT;
    stateMachine.lastChangestateTime = HAL_GetTick();

    createMqttConnection( mqtt_handle.id, mqtt_handle.user, mqtt_handle.psw );
};

void set_error_state () {
    stateMachine.state = WATING_SIM;
    stateMachine.timeout = STATE_TIMEOUT;
    stateMachine.lastChangestateTime = HAL_GetTick();

    /* Sends a command as soon as it change state */
    sendAT("AT+CFUN=1,1\r\n", NULL);

    sprintf( stateMachine.command, "AT+CIPSTATUS\r\n" );
    setExpected((char *)"\r\nOK\r\n\r\nSTATE: IP INITIAL\r\n");
}

void passive_states_handdler () {
    switch  ( stateMachine.state )
    {
        case ERROR_STATE:
            break;
        
        case WAITING_SIM:
            break;

        case IDLE_DISCONNECTED:
            break;

        case TCP_CONNECTING:
            break;

        case CONNECT_OK:
            break;

        case TCP_CLOSED:
            break;
    }
}

int is_error_count_out () {
    if ( stateMachine.error_count >= ERR_COUNT_OUT ) {

        stateMachine.error_count = 0;
        return 1;
    }

    return 0;
}

void HAL_UARTEx_RxEventCallback ( UART_HandleTypeDef *huart, uint16_t Size )
{
    stateMachine.incomingMsgSize = Size;
    stateMachine.incomingMsg[ Size ] = '\0';
    stateMachine.interruptFlag = 1;
}

void HAL_UART_ErrorCallback ( UART_HandleTypeDef * huart ) {
    stateMachine.clearAllFlag = 1;
} 

void clearUARTCtrl ( UART_HandleTypeDef *huart ) {
    if ( stateMachine.clearAllFlag )
    {   
        memset( stateMachine.incomingMsg, '\0', sizeof(stateMachine.incomingMsg) );
        HAL_UARTEx_ReceiveToIdle_IT(
            stateMachine.huartAT,
            (unsigned char *) stateMachine.incomingMsg,
            sizeof( stateMachine.incomingMsg )
        );
        
        stateMachine.clearAllFlag = 0;
    }
}

void resolveUARTCtrl ( UART_HandleTypeDef *huart ) {

    if ( stateMachine.interruptFlag ) {
        stateMachine.interruptFlag = 0;

        // Checks if this callback is raised by the GSM serial
        if ( huart != stateMachine.huartAT ) {
            // memset( stateMachine.incomingMsg, '\0', sizeof(stateMachine.incomingMsg) );
            HAL_UARTEx_ReceiveToIdle_IT(
                stateMachine.huartAT,
                (unsigned char *) stateMachine.incomingMsg,
                sizeof( stateMachine.incomingMsg )
            );
            return;
        }
            
        //  Checks if message starts with \n (initial message mark)
        if ( stateMachine.incomingMsg[0] != '\r'  ) {
            // memset( stateMachine.incomingMsg, '\0', sizeof(stateMachine.incomingMsg) );
            HAL_UARTEx_ReceiveToIdle_IT(
                stateMachine.huartAT,
                (unsigned char *) stateMachine.incomingMsg,
                sizeof( stateMachine.incomingMsg )
            );
            return;
        }

        // =================================================================================

        // If success, check 
        switch (stateMachine.state)
        {

        case WAITING_SIM:
            if ( strncmp( stateMachine.incomingMsg, "" ) )

        case IDLE_DISCONNECTED:
            if ( strncmp(stateMachine.incomingMsg, stateMachine.resume_msg, 11U) == 0 )
            {
                /* Reset error count */
                stateMachine.error_count = 0;
                set_ip_initial_state();
            }
            else if ( strncmp(stateMachine.incomingMsg, stateMachine.reject_msg, 9U) == 0 )
            {
                stateMachine.error_count++;
                stateMachine.lastChangestateTime = 0;
                if ( is_error_count_out() ) is_error_count_out();
                
            } else if ( strncmp(stateMachine.incomingMsg, "\r\n+PDP: DEACT\r\n\r\nERROR\r\n", 9U) == 0 ) {
                sendAT("AT+CIPSHUT\r\n", NULL);
            }
            break;
        
        case IP_INITIAL:
            if ( strncmp(stateMachine.incomingMsg, stateMachine.resume_msg, 4U) == 0 )
            {
                /* Reset error count */
                stateMachine.error_count = 0;
                set_ip_start_state();
            }
            else if ( strncmp(stateMachine.incomingMsg, stateMachine.reject_msg, 9U) == 0 )
            {
                stateMachine.error_count++;
                stateMachine.lastChangestateTime = 0;
                if ( is_error_count_out() ) set_error_state();
            }

            break;

        case IP_START:
            if ( strncmp(stateMachine.incomingMsg, stateMachine.resume_msg, 4U) == 0 )
            {
                /* Reset error count */
                stateMachine.error_count = 0;
                set_ip_gprsact_state();
            }
            else if ( strncmp(stateMachine.incomingMsg, stateMachine.reject_msg, 9U) == 0 )
            {
                stateMachine.error_count++;
                stateMachine.lastChangestateTime = 0;
                if ( is_error_count_out() ) set_error_state();
            }

            break;

        case IP_GPRSACT:

            if ( sscanf(stateMachine.incomingMsg, "\n%u.%u.%u.%u", &ip_a, &ip_b, &ip_c, &ip_d) == 4 )
            {
                /* Reset error count */
                stateMachine.error_count = 0;
                set_ip_status_state();
            }
            else if ( strncmp(stateMachine.incomingMsg, stateMachine.reject_msg, 9U) == 0 )
            {
                stateMachine.error_count++;
                stateMachine.lastChangestateTime = 0;
                if ( is_error_count_out() )
                    set_error_state();
                else
                    set_ip_start_state();
            }

            break;

        case IP_STATUS:
            if ( strncmp(stateMachine.incomingMsg, stateMachine.resume_msg, 4U) == 0 )
            {
                /* Reset error count */
                stateMachine.error_count = 0;
                set_tcp_connecting_state();
            }
            else if ( strncmp(stateMachine.incomingMsg, stateMachine.reject_msg, 9U) == 0 )
            {
                stateMachine.error_count++;
                stateMachine.lastChangestateTime = 0;
                if ( is_error_count_out() )
                    set_error_state();
                else
                    set_ip_gprsact_state();
            }

            break;

        case TCP_CONNECTING:
            if (    strncmp( stateMachine.incomingMsg, (char *)"\r\nCONNECT OK", 11U ) == 0
                    || strncmp( stateMachine.incomingMsg, (char *)"\r\nALREADY CONNECT", 16U ) == 0 )
            {
                /* Reset error count */
                stateMachine.error_count = 0;
                set_connect_ok_state();
            }
            else if ( 
                strncmp(stateMachine.incomingMsg, "\r\nTCP CLOSED", 18U) == 0
                || strncmp(stateMachine.incomingMsg, "\r\nCONNECT FAIL", 13U) == 0
            ){
                stateMachine.error_count++;
                stateMachine.lastChangestateTime = 0;
                if ( is_error_count_out() )
                    set_error_state();
                else
                    set_ip_status_state();
            }

            break;

        case CONNECT_OK:
            if ( strncmp( stateMachine.incomingMsg, "\r\nCLOSED", 7U ) == 0 )
                set_ip_status_state();
            
            if ( strncmp( stateMachine.incomingMsg, "\r\n>", 3U ) == 0 && mqtt_handle.availableToSend){
                
                memcpy( msgBuff, mqtt_handle.msgBuff, mqtt_handle.totalMsgLen );
                msgBuff[ mqtt_handle.totalMsgLen ] = 0x1A;

                sendAT( msgBuff, mqtt_handle.totalMsgLen );

                mqtt_handle.availableToSend = 0;
            }

            break;
        
        case TCP_CLOSED:
            // Do something -\_('-')_/-
            break;

        case ERROR_STATE:
            // Do something -\_('-')_/-
            break;

        default:
            break;
        }
        
        // memset( stateMachine.incomingMsg, '\0', sizeof(stateMachine.incomingMsg) );
        HAL_UARTEx_ReceiveToIdle_IT(
            stateMachine.huartAT,
            (unsigned char *) stateMachine.incomingMsg,
            sizeof( stateMachine.incomingMsg )
        );

    }
}