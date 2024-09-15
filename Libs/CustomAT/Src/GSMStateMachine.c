#include "custom-at.h"
#include "custom-mqtt.h"
#include <string.h>

int ip_a;
int ip_b;
int ip_c;
int ip_d;
char * msgBuff;

extern ATmssg_t GSMstateMachine; // Defined here but declared at file: ./custom-at.c
extern mqt_t mqtt_handle;     // Defined here but declared at file ../../custom-mqtt.c

void set_ip_initial_state() {
    sprintf( GSMstateMachine.command, "AT+CIPSTATUS\r\n" );
    setExpected((char *)"\r\nOK\r\n\r\nSTATE: IP INITIAL\r\n");
    GSMstateMachine.state = IP_INITIAL;
    GSMstateMachine.timeout = STATE_TIMEOUT;
    GSMstateMachine.lastChangestateTime = HAL_GetTick();
    sprintf( GSMstateMachine.command, "AT+CSTT=\"%s\",\"%s\",\"%s\"\r\n", GSMstateMachine.apn_host, GSMstateMachine.apn_user, GSMstateMachine.apn_psw );

    /* Sends a command as soon as it change state */
    sendAT( GSMstateMachine.command );
};

void set_ip_start_state() {
    setExpected((char *)"\r\nOK\r\n");
    GSMstateMachine.state = IP_START;
    GSMstateMachine.timeout = CIICR_TIMEOUT;
    GSMstateMachine.lastChangestateTime = HAL_GetTick();
    sprintf( GSMstateMachine.command, "AT+CIICR\r\n" );

    /* Sends a command as soon as it change state */
    sendAT( GSMstateMachine.command );
};

void set_ip_gprsact_state() {
    setExpected((char *)"\r\nOK\r\n");
    GSMstateMachine.state = IP_GPRSACT;
    GSMstateMachine.timeout = STATE_TIMEOUT;
    GSMstateMachine.lastChangestateTime = HAL_GetTick();
    sprintf( GSMstateMachine.command, "AT+CIFSR\r\n" );

    /* Sends a command as soon as it change state */
    sendAT( GSMstateMachine.command );
};

void set_ip_status_state() {
    setExpected((char *)"\r\nOK\r\n");
    GSMstateMachine.state = IP_STATUS;
    GSMstateMachine.timeout = STATE_TIMEOUT;
    GSMstateMachine.lastChangestateTime = HAL_GetTick();
    sprintf( GSMstateMachine.command, "AT+CIPSTART=\"TCP\",\"%s\",%d\r\n", GSMstateMachine.mqtt_host, GSMstateMachine.mqtt_port );

    /* Sends a command as soon as it change state */
    sendAT( GSMstateMachine.command );
};

void set_tcp_connecting_state() {
    GSMstateMachine.state = TCP_CONNECTING;
    GSMstateMachine.timeout = INFINITE_TIMEOUT;
    GSMstateMachine.lastChangestateTime = HAL_GetTick();
};

void set_connect_ok_state() {
    GSMstateMachine.state = CONNECT_OK;
    GSMstateMachine.timeout = INFINITE_TIMEOUT;
    GSMstateMachine.lastChangestateTime = HAL_GetTick();

    createMqttConnection( mqtt_handle.id, mqtt_handle.user, mqtt_handle.psw );
};

void set_error_state () {
    GSMstateMachine.state = IDLE_DISCONNECTED;
    GSMstateMachine.timeout = STATE_TIMEOUT;
    GSMstateMachine.lastChangestateTime = HAL_GetTick();

    /* Sends a command as soon as it change state */
    sendAT("AT+CFUN=1,1\r\n");

    sprintf( GSMstateMachine.command, "AT+CIPSTATUS\r\n" );
    setExpected((char *)"\r\nOK\r\n\r\nSTATE: IP INITIAL\r\n");
}

int is_error_count_out () {
    if ( GSMstateMachine.error_count >= ERR_COUNT_OUT ) {

        GSMstateMachine.error_count = 0;
        return 1;
    }

    return 0;
}

/**
 * @brief This function serves as a uart callback specifically for the gsm callbacks. When the
 * HAL_UARTEx_RxEventCallback is called by the hardware, it should redirect to this function if
 * huart->Instance = gsm_huart
 * 
 * @param Size 
 */
void gsm_uart_callback ( UART_HandleTypeDef * huart, uint16_t Size )
{
    GSMstateMachine.incomingMsgSize = Size;
    GSMstateMachine.incomingMsg[ Size ] = '\0';
    GSMstateMachine.interruptFlag = 1;
}

void HAL_UART_ErrorCallback ( UART_HandleTypeDef * huart ) {
    GSMstateMachine.clearAllFlag = 1;
} 

void clearUARTCtrl ( UART_HandleTypeDef *huart ) {
    if ( GSMstateMachine.clearAllFlag )
    {   
        memset( GSMstateMachine.incomingMsg, '\0', sizeof(GSMstateMachine.incomingMsg) );
        HAL_UARTEx_ReceiveToIdle_IT(
            GSMstateMachine.huartAT,
            (unsigned char *) GSMstateMachine.incomingMsg,
            sizeof( GSMstateMachine.incomingMsg )
        );
        
        GSMstateMachine.clearAllFlag = 0;
    }
}

void resolveUARTCtrl ( UART_HandleTypeDef *huart ) {

    if ( GSMstateMachine.interruptFlag ) {
        GSMstateMachine.interruptFlag = 0;

        // Checks if this callback is raised by the GSM serial
        if ( huart != GSMstateMachine.huartAT ) {
            // memset( GSMstateMachine.incomingMsg, '\0', sizeof(GSMstateMachine.incomingMsg) );
            HAL_UARTEx_ReceiveToIdle_IT(
                GSMstateMachine.huartAT,
                (unsigned char *) GSMstateMachine.incomingMsg,
                sizeof( GSMstateMachine.incomingMsg )
            );
            return;
        }
            
        //  Checks if message starts with \n (initial message mark)
        if ( GSMstateMachine.incomingMsg[0] != '\r'  ) {
            // memset( GSMstateMachine.incomingMsg, '\0', sizeof(GSMstateMachine.incomingMsg) );
            HAL_UARTEx_ReceiveToIdle_IT(
                GSMstateMachine.huartAT,
                (unsigned char *) GSMstateMachine.incomingMsg,
                sizeof( GSMstateMachine.incomingMsg )
            );
            return;
        }

        // =================================================================================

        // If success, check 
        switch (GSMstateMachine.state)
        {

        case IDLE_DISCONNECTED:
            if ( strncmp(GSMstateMachine.incomingMsg, GSMstateMachine.resume_msg, 11U) == 0 )
            {
                /* Reset error count */
                GSMstateMachine.error_count = 0;
                set_ip_initial_state();
            }
            else if ( strncmp(GSMstateMachine.incomingMsg, GSMstateMachine.reject_msg, 9U) == 0 )
            {
                GSMstateMachine.error_count++;
                GSMstateMachine.lastChangestateTime = 0;
                if ( is_error_count_out() ) is_error_count_out();
            } else if ( strncmp(GSMstateMachine.incomingMsg, "\r\n+PDP: DEACT\r\n\r\nERROR\r\n", 9U) == 0 ) {
                sendAT("AT+CIPSHUT\r\n");
            }
            break;
        
        case IP_INITIAL:
            if ( strncmp(GSMstateMachine.incomingMsg, GSMstateMachine.resume_msg, 4U) == 0 )
            {
                /* Reset error count */
                GSMstateMachine.error_count = 0;
                set_ip_start_state();
            }
            else if ( strncmp(GSMstateMachine.incomingMsg, GSMstateMachine.reject_msg, 9U) == 0 )
            {
                GSMstateMachine.error_count++;
                GSMstateMachine.lastChangestateTime = 0;
                if ( is_error_count_out() ) set_error_state();
            }

            break;

        case IP_START:
            if ( strncmp(GSMstateMachine.incomingMsg, GSMstateMachine.resume_msg, 4U) == 0 )
            {
                /* Reset error count */
                GSMstateMachine.error_count = 0;
                set_ip_gprsact_state();
            }
            else if ( strncmp(GSMstateMachine.incomingMsg, GSMstateMachine.reject_msg, 9U) == 0 )
            {
                GSMstateMachine.error_count++;
                GSMstateMachine.lastChangestateTime = 0;
                if ( is_error_count_out() ) set_error_state();
            }

            break;

        case IP_GPRSACT:

            if ( sscanf(GSMstateMachine.incomingMsg, "\n%u.%u.%u.%u", &ip_a, &ip_b, &ip_c, &ip_d) == 4 )
            {
                /* Reset error count */
                GSMstateMachine.error_count = 0;
                set_ip_status_state();
            }
            else if ( strncmp(GSMstateMachine.incomingMsg, GSMstateMachine.reject_msg, 9U) == 0 )
            {
                GSMstateMachine.error_count++;
                GSMstateMachine.lastChangestateTime = 0;
                if ( is_error_count_out() )
                    set_error_state();
                else
                    set_ip_start_state();
            }

            break;

        case IP_STATUS:
            if ( strncmp(GSMstateMachine.incomingMsg, GSMstateMachine.resume_msg, 4U) == 0 )
            {
                /* Reset error count */
                GSMstateMachine.error_count = 0;
                set_tcp_connecting_state();
            }
            else if ( strncmp(GSMstateMachine.incomingMsg, GSMstateMachine.reject_msg, 9U) == 0 )
            {
                GSMstateMachine.error_count++;
                GSMstateMachine.lastChangestateTime = 0;
                if ( is_error_count_out() )
                    set_error_state();
                else
                    set_ip_gprsact_state();
            }

            break;

        case TCP_CONNECTING:
            if (    strncmp( GSMstateMachine.incomingMsg, (char *)"\r\nCONNECT OK", 11U ) == 0
                    || strncmp( GSMstateMachine.incomingMsg, (char *)"\r\nALREADY CONNECT", 16U ) == 0 )
            {
                /* Reset error count */
                GSMstateMachine.error_count = 0;
                set_connect_ok_state();
            }
            else if ( 
                strncmp(GSMstateMachine.incomingMsg, "\r\nTCP CLOSED", 18U) == 0
                || strncmp(GSMstateMachine.incomingMsg, "\r\nCONNECT FAIL", 13U) == 0
            ){
                GSMstateMachine.error_count++;
                GSMstateMachine.lastChangestateTime = 0;
                if ( is_error_count_out() )
                    set_error_state();
                else
                    set_ip_status_state();
            }

            break;

        case CONNECT_OK:
            if ( strncmp( GSMstateMachine.incomingMsg, "\r\nCLOSED", 7U ) == 0 )
                set_ip_status_state();
            
            if ( strncmp( GSMstateMachine.incomingMsg, "\r\n>", 3U ) == 0 && mqtt_handle.availableToSend){
                msgBuff = mqtt_handle.msgBuff; 
                sendAT( msgBuff );
                sendAT((char *)0x1A);
                mqtt_handle.availableToSend = 0;
            }

            break;
        
        case TCP_CLOSED:
            // Do something -\_('-')_/-

            break;

        default:
            break;
        }
        
        // memset( GSMstateMachine.incomingMsg, '\0', sizeof(GSMstateMachine.incomingMsg) );
        HAL_UARTEx_ReceiveToIdle_IT(
            GSMstateMachine.huartAT,
            (unsigned char *) GSMstateMachine.incomingMsg,
            sizeof( GSMstateMachine.incomingMsg )
        );

    }
}