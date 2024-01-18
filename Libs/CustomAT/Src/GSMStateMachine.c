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

void set_waiting_sim_state ()
{
    sprintf( stateMachine.command, "AT\r\n" );
    setExpected ((char *)"\r\nSMS Ready\r\n");
    stateMachine.state = WAITING_SIM;
    stateMachine.timeout = STATE_TIMEOUT;
    stateMachine.lastChangestateTime = HAL_GetTick();
}

void set_idle_disconnected ()
{
    sprintf( stateMachine.command, "AT+CIPSTATUS\r\n" );
    setExpected((char *)"\r\nOK\r\n\r\nSTATE: IP INITIAL\r\n");
    stateMachine.state = IDLE_DISCONNECTED;
    stateMachine.timeout = STATE_TIMEOUT;
    stateMachine.lastChangestateTime = HAL_GetTick();
}

void set_ip_initial_state ()
{
    setExpected((char *)"\r\nOK\r\n");
    stateMachine.state = IP_INITIAL;
    stateMachine.timeout = STATE_TIMEOUT;
    stateMachine.lastChangestateTime = HAL_GetTick();
    sprintf( stateMachine.command, "AT+CSTT=\"%s\",\"%s\",\"%s\"\r\n", stateMachine.apn_host, stateMachine.apn_user, stateMachine.apn_psw );

    /* Sends a command as soon as it change state */
    sendAT( stateMachine.command, (size_t)NULL );
};

void set_ip_start_state ()
{
    setExpected((char *)"\r\nOK\r\n");
    stateMachine.state = IP_START;
    stateMachine.timeout = CIICR_TIMEOUT;
    stateMachine.lastChangestateTime = HAL_GetTick();
    sprintf( stateMachine.command, "AT+CIICR\r\n" );

    /* Sends a command as soon as it change state */
    sendAT( stateMachine.command, (size_t)NULL );
};

void set_ip_gprsact_state ()
{
    setExpected((char *)"\r\nOK\r\n");
    stateMachine.state = IP_GPRSACT;
    stateMachine.timeout = STATE_TIMEOUT;
    stateMachine.lastChangestateTime = HAL_GetTick();
    sprintf( stateMachine.command, "AT+CIFSR\r\n" );

    /* Sends a command as soon as it change state */
    sendAT( stateMachine.command, (size_t)NULL );
};

void set_ip_status_state ()
{
    setExpected((char *)"\r\nOK\r\n");
    stateMachine.state = IP_STATUS;
    stateMachine.timeout = STATE_TIMEOUT;
    stateMachine.lastChangestateTime = HAL_GetTick();
    sprintf( stateMachine.command, "AT+CIPSTART=\"TCP\",\"%s\",%d\r\n", stateMachine.mqtt_host, stateMachine.mqtt_port );

    /* Sends a command as soon as it change state */
    sendAT( stateMachine.command, (size_t)NULL );
};

void set_tcp_connecting_state ()
{
    stateMachine.state = TCP_CONNECTING;
    stateMachine.timeout = INFINITE_TIMEOUT;
    stateMachine.lastChangestateTime = HAL_GetTick();
};

void set_connect_ok_state ()
{
    stateMachine.state = CONNECT_OK;
    stateMachine.timeout = INFINITE_TIMEOUT;
    stateMachine.lastChangestateTime = HAL_GetTick();

    /* Here, the mqtt connect packet will be created and the mqtt_handle.availableToSend will be set to 1 for the first time */
    createMqttConnection( mqtt_handle.id, mqtt_handle.user, mqtt_handle.psw );
};

void set_error_state ()
{
    stateMachine.state = ERROR_STATE;
    stateMachine.timeout = STATE_TIMEOUT;
    stateMachine.lastChangestateTime = HAL_GetTick();
}

int is_error_count_out ( int value )
{
    if ( value != (int)NULL ) {
        if ( stateMachine.error_count >= value ) {

            stateMachine.error_count = 0;
            return 1;
        }
    } else
    {
        if ( stateMachine.error_count >= ERR_COUNT_OUT_STD ) {

            stateMachine.error_count = 0;
            return 1;
        }
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

/**
 * @brief Fucntion to act according to the received UART message. It handdles the GSM message frame validation and
 *        state changes. It is controlled by the stateMachine.interruptFlag message flag. If this variable is set
 *        by the UART ISR, then resolve this function resolves the state changes and unset the interruptFlag again.
 *        Check the following link for the state machine information:
 *        https://www.figma.com/file/mak4llwORDq15pVzqMm7mB/UWB-Ideas?type=whiteboard&node-id=0%3A1&t=Hj7Z12aJQdf3mpsE-1
 * 
 * @param huart 
 */
void resolveUARTCtrl ( UART_HandleTypeDef *huart )
{

    if ( stateMachine.interruptFlag ) {

        // Checks if this callback is raised by the GSM serial
        if ( huart != stateMachine.huartAT ) {
            HAL_UARTEx_ReceiveToIdle_IT(
                stateMachine.huartAT,
                (unsigned char *) stateMachine.incomingMsg,
                sizeof( stateMachine.incomingMsg )
            );
            return;
        }

        //  Checks if message starts with \r\n (initial GSM response message mark)
        if ( stateMachine.incomingMsg[0] != '\r' && stateMachine.incomingMsg[1] != '\n'  ) {
            HAL_UARTEx_ReceiveToIdle_IT(
                stateMachine.huartAT,
                (unsigned char *) stateMachine.incomingMsg,
                sizeof( stateMachine.incomingMsg )
            );
            return;
        }

        //  Check if it is a SIM800L request for MCU input ">"
        if ( strncmp( stateMachine.incomingMsg, (const char *)"\r\n>", 3U ) == 0 )
        {

            if ( mqtt_handle.availableToSend )
            {
                /* Send the command waiting on the mqtt_handle.msgBuff */
                sendAT( mqtt_handle.msgBuff, mqtt_handle.totalMsgLen + 3 );
                /* Unflag the message ready flag */
                mqtt_handle.availableToSend = 0;

                /* Restart the RX receive */
                HAL_UARTEx_ReceiveToIdle_IT(
                    stateMachine.huartAT,
                    (unsigned char *) stateMachine.incomingMsg,
                    sizeof( stateMachine.incomingMsg )
                );
                return;
            }
        }

        /* ============================== STATE CONTROL BLOCK ============================== */
        switch (stateMachine.state)
        {

        case WAITING_SIM:
            if ( strncmp( stateMachine.incomingMsg, stateMachine.resume_msg, 11U ) == 0 )
            {
                stateMachine.error_count = 0;
                set_idle_disconnected();
            }
            else if ( strncmp( stateMachine.incomingMsg, ( const char * )"\r\nOK", 4) == 0 )
            {
                stateMachine.error_count++;
                if ( is_error_count_out( ERR_COUNT_OUT_WAITING_SIM ) ) set_error_state();
            }
            break;

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
                if ( is_error_count_out((int)NULL)  ) set_error_state();
                
            } else if ( strncmp(stateMachine.incomingMsg, "\r\n+PDP: DEACT\r\n\r\nERROR\r\n", 9U) == 0 ) {
                sendAT("AT+CIPSHUT\r\n", (size_t)NULL);
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
                if ( is_error_count_out((int)NULL)  ) set_error_state();
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
                if ( is_error_count_out((int)NULL)  ) set_error_state();
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
                if ( is_error_count_out((int)NULL)  )
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
                if ( is_error_count_out((int)NULL)  )
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
                if ( is_error_count_out((int)NULL)  )
                    set_error_state();
                else
                    set_ip_status_state();
            }

            break;

        case CONNECT_OK:
            if ( strncmp( stateMachine.incomingMsg, "\r\nERROR\r\n\r\nCLOSED", 17U ) == 0 )
            {
                set_ip_status_state();
                mqtt_handle.connected = 0;
            }

            if ( strncmp( stateMachine.incomingMsg, "\r\nERROR", 7U ) == 0 )
            {
                if ( is_error_count_out((int)NULL)  )
                    set_error_state();
                else
                    set_ip_status_state();
            }

            if ( strncmp( stateMachine.incomingMsg, "\r\nSEND OK", 9U ) == 0 )
            {
                mqtt_handle.connected = 1;
            }

            break;

        default:
            break;
        }


        /* Restart the Rx interrupt routine */
        HAL_UARTEx_ReceiveToIdle_IT(
            stateMachine.huartAT,
            (unsigned char *) stateMachine.incomingMsg,
            sizeof( stateMachine.incomingMsg )
        );

        /* Unset the UART Rx interrupt flag */
        stateMachine.interruptFlag = 0;

    }
}