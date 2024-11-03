#include "custom-at.h"
#include "custom-mqtt.h"
#include <string.h>

int ip_a;
int ip_b;
int ip_c;
int ip_d;
char * msgBuff;

extern ATmssg_t GSMstateMachine; // Defined here but declared at file: ./custom-at.c
extern mqt_t mqtt_handler;     // Defined here but declared at file ../../custom-mqtt.c

void set_initial_state() {
    
    resetGSM();

    GSMstateMachine.state = ERROR_STATE;
    GSMstateMachine.timeout = 500;//CFUN_TIME;
    GSMstateMachine.err_countout = 15;//3
    GSMstateMachine.lastChangestateTime = HAL_GetTick() - 500;

    sprintf( GSMstateMachine.command, "ATE0&W;+CFUN=1,1\r\n" ); // Disable the at command echo back and restart the moden
    GSMstateMachine.command_len = 21;
    setExpected((char *)"\r\nOK\r\n");
    setRejectExpected((char *)"\r\nERROR\r\n");
    
}

void set_ip_initial_state() {

    setExpected((char *)"\r\nOK\r\n");
    GSMstateMachine.state = IP_INITIAL;
    GSMstateMachine.timeout = 2 * STATE_TIMEOUT;
    GSMstateMachine.err_countout = 3;
    GSMstateMachine.lastChangestateTime = HAL_GetTick();

    /* sends a command as soon as it change state */
    sprintf( GSMstateMachine.command, "AT+CSTT=\"%s\",\"%s\",\"%s\"\r\n", GSMstateMachine.apn_host, GSMstateMachine.apn_user, GSMstateMachine.apn_psw );
    GSMstateMachine.command_len = 18 + strlen(GSMstateMachine.apn_host) + strlen(GSMstateMachine.apn_user) + strlen(GSMstateMachine.apn_psw);

    // GSMstateMachine.lastChangestateTime = HAL_GetTick() - STATE_TIMEOUT;
};

void set_idle_disconnected_state() {
    sprintf( GSMstateMachine.command, "AT+CIPSTATUS\r\n" );
    GSMstateMachine.command_len = 14;
    setExpected((char *)"\r\nOK\r\n\r\nSTATE: IP INITIAL\r\n");
    GSMstateMachine.state = IDLE_DISCONNECTED;
    GSMstateMachine.timeout = 2 * STATE_TIMEOUT;
    GSMstateMachine.err_countout = 3;
    GSMstateMachine.lastChangestateTime = HAL_GetTick();

    /* sends a command as soon as it change state */
    sendAT( GSMstateMachine.command, GSMstateMachine.command_len );
}

void set_ip_start_state() {
    setExpected((char *)"\r\nOK\r\n");
    GSMstateMachine.state = IP_START;
    GSMstateMachine.timeout = CIICR_TIMEOUT;
    GSMstateMachine.err_countout = 3;
    // GSMstateMachine.lastChangestateTime = HAL_GetTick();
    sprintf( GSMstateMachine.command, "AT+CIICR\r\n" );
    GSMstateMachine.command_len = 10;

    /* Sends a command as soon as it change state */
    GSMstateMachine.lastChangestateTime = HAL_GetTick() -  CIICR_TIMEOUT;
    // sendAT( GSMstateMachine.command, GSMstateMachine.command_len );
};

void set_ip_gprsact_state() {
    setExpected((char *)"\r\nOK\r\n");
    GSMstateMachine.state = IP_GPRSACT;
    GSMstateMachine.timeout = STATE_TIMEOUT;
    GSMstateMachine.err_countout = 3;
    GSMstateMachine.lastChangestateTime = HAL_GetTick();
    sprintf( GSMstateMachine.command, "AT+CIFSR\r\n" );
    GSMstateMachine.command_len = 10;

    /* Sends a command as soon as it change state */
    sendAT( GSMstateMachine.command, GSMstateMachine.command_len );
};

void set_ip_status_state() {
    setExpected((char *)"\r\nOK\r\n");
    GSMstateMachine.state = IP_STATUS;
    GSMstateMachine.timeout = CIPSTART_TIMEOUT;
    GSMstateMachine.err_countout = 2;//2
    GSMstateMachine.lastChangestateTime = HAL_GetTick();
    GSMstateMachine.time_to_check_connection = HAL_GetTick(); // Resets the timer for online connection check
    sprintf( GSMstateMachine.command, "AT+CIPSTART=\"TCP\",\"%s\",%d\r\n", GSMstateMachine.mqtt_host, GSMstateMachine.mqtt_port );
    GSMstateMachine.command_len = 23 + strlen(GSMstateMachine.mqtt_host) + 4;

    /* Sends a command as soon as it change state */
    sendAT( GSMstateMachine.command, GSMstateMachine.command_len );
};

void set_tcp_connecting_state() {
    GSMstateMachine.state = TCP_CONNECTING;
    GSMstateMachine.timeout = INFINITE_TIMEOUT;
    GSMstateMachine.err_countout = 3;
    GSMstateMachine.lastChangestateTime = HAL_GetTick();
};

void set_connect_ok_state() {
    GSMstateMachine.state = CONNECT_OK;
    GSMstateMachine.timeout = INFINITE_TIMEOUT;
    GSMstateMachine.err_countout = 3;
    GSMstateMachine.lastChangestateTime = HAL_GetTick();
};

void set_error_state () {
    GSMstateMachine.state = ERROR_STATE;
    GSMstateMachine.timeout = 500;//CFUN_TIME;
    GSMstateMachine.err_countout = 15;//3
    GSMstateMachine.lastChangestateTime = HAL_GetTick();

    sprintf( GSMstateMachine.command, "ATE0&W;+CFUN=1,1\r\n" );
    GSMstateMachine.command_len = 21;
    setExpected((char *)"\r\nOK\r\n");

    /* Sends a command as soon as it change state */
    sendAT( GSMstateMachine.command, GSMstateMachine.command_len );
}

void set_sending_msg_state () {
    GSMstateMachine.state = SENDING_MSG;
    GSMstateMachine.timeout = HAL_MAX_DELAY;
    GSMstateMachine.err_countout = 3;
    GSMstateMachine.lastChangestateTime = HAL_GetTick();
}

int32_t wait_after_restart;
void set_waiting_sim_state () {
    wait_after_restart = HAL_GetTick(); // To control how much time this state change would wait after modem reset
    setExpected((char *)"\r\nSMS Ready\r\n");
    GSMstateMachine.state = WAITING_SIM;
    GSMstateMachine.timeout = CPIN_TIME;//2* CPIN_TIME;
    GSMstateMachine.err_countout = 3;//3
    sprintf( GSMstateMachine.command, "AT+CPIN=%d\r\n", GSMstateMachine.pin );
    GSMstateMachine.command_len = 10 + 4;

    /* Sends a command as soon as it change state */
    // GSMstateMachine.lastChangestateTime = HAL_GetTick() - (CPIN_TIME * 0.8 );//(-2) * CPIN_TIME;
    GSMstateMachine.lastChangestateTime = HAL_GetTick();
    // sendAT( GSMstateMachine.command, GSMstateMachine.command_len );
}

int is_error_count_out ()
{
        if ( GSMstateMachine.error_count >= GSMstateMachine.err_countout ) {
            GSMstateMachine.error_count = 0;
            return 1;
        }

    return 0;
}

int is_state_timeout () {
    int is_timeout = ( HAL_GetTick() - GSMstateMachine.lastChangestateTime) > GSMstateMachine.timeout;
    if (is_timeout)
        GSMstateMachine.lastChangestateTime = HAL_GetTick();
    
    return is_timeout;
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

    //  Resolves the GSM incommign UART packets
    if ( GSMstateMachine.interruptFlag ) {  // checks if there is message on the receiver buffer
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
        GSMstateMachine.modem_is_alive = 1; // Signals that GSM modem is still responding and reset check interval timers
        GSMstateMachine.time_to_check_modem = HAL_GetTick();
        GSMstateMachine.time_to_check_connection = HAL_GetTick();
        // If success, check 
        switch (GSMstateMachine.state)
        {
        case WAITING_SIM:
            if ( strstr( GSMstateMachine.incomingMsg, "+CPIN: READY" ) != NULL
                    || strstr( GSMstateMachine.incomingMsg, "Call Ready" ) != NULL ) {
                        GSMstateMachine.disable_state_timeout = 1; // Disable state timeout. Int that way, either the "SMS Ready" or GSM response timeout
                        break;
            }
            if ( strncmp( GSMstateMachine.incomingMsg, GSMstateMachine.resume_msg, 11U ) == 0) {
                GSMstateMachine.disable_state_timeout = 0; // Enable back the state timeout
                /* Reset error count */
                GSMstateMachine.error_count = 0;
                set_idle_disconnected_state();
            }
            else if ( strstr( GSMstateMachine.incomingMsg, GSMstateMachine.reject_msg ) != NULL ) {
                GSMstateMachine.error_count++;  // If reject message
                sendAT( GSMstateMachine.command, GSMstateMachine.command_len ); // Immediately re-send command as soon as the previous fails
                GSMstateMachine.lastChangestateTime = HAL_GetTick(); // reset this state last time changed
            }

            break;

        case IDLE_DISCONNECTED:
            if ( strncmp(GSMstateMachine.incomingMsg, GSMstateMachine.resume_msg, 25U) == 0 )
            {
                /* Reset error count */
                GSMstateMachine.error_count = 0;
                set_ip_initial_state();
            }
            else if ( strncmp(GSMstateMachine.incomingMsg, GSMstateMachine.reject_msg, 9U) == 0 ){
                GSMstateMachine.error_count++;  // If reject message
                sendAT( GSMstateMachine.command, GSMstateMachine.command_len ); // Immediately re-send command as soon as the previous fails
                GSMstateMachine.lastChangestateTime = HAL_GetTick(); // reset this state last time changed
            }
            else
                GSMstateMachine.error_count++;  // Any other behaviour

            break;
        
        case IP_INITIAL:
            if ( strncmp(GSMstateMachine.incomingMsg, GSMstateMachine.resume_msg, 4U) == 0 )
            {
                /* Reset error count */
                GSMstateMachine.error_count = 0;
                set_ip_start_state();
            }
            else if ( strncmp(GSMstateMachine.incomingMsg, GSMstateMachine.reject_msg, 9U) == 0 ){
                GSMstateMachine.error_count++;  // If reject message
                sendAT( GSMstateMachine.command, GSMstateMachine.command_len ); // Immediately re-send command as soon as the previous fails
                GSMstateMachine.lastChangestateTime = HAL_GetTick(); // reset this state last time changed
            }
            else
                GSMstateMachine.error_count++;      // Any other behaviour

            break;

        case IP_START:
            if ( strncmp(GSMstateMachine.incomingMsg, GSMstateMachine.resume_msg, 4U) == 0 )
            {
                /* Reset error count */
                GSMstateMachine.error_count = 0;
                set_ip_gprsact_state();
            }
            else if ( strstr(GSMstateMachine.incomingMsg, GSMstateMachine.reject_msg) != NULL ){
                GSMstateMachine.error_count++;  // If reject message
                sendAT( GSMstateMachine.command, GSMstateMachine.command_len ); // Immediately re-send command as soon as the previous fails
                GSMstateMachine.lastChangestateTime = HAL_GetTick(); // reset this state last time changed
            }
            else
                GSMstateMachine.error_count++;      // Any other behaviour

            break;

        case IP_GPRSACT:

            if ( sscanf(GSMstateMachine.incomingMsg, "\n%u.%u.%u.%u", &ip_a, &ip_b, &ip_c, &ip_d) == 4 )
            {
                /* Reset error count */
                GSMstateMachine.error_count = 0;
                set_ip_status_state();
            }
            else if ( strncmp(GSMstateMachine.incomingMsg, GSMstateMachine.reject_msg, 9U) == 0 ){
                GSMstateMachine.error_count++;  // If reject message
                sendAT( GSMstateMachine.command, GSMstateMachine.command_len ); // Immediately re-send command as soon as the previous fails
                GSMstateMachine.lastChangestateTime = HAL_GetTick(); // reset this state last time changed
            }
            else
                GSMstateMachine.error_count++;      // Any other behaviour

            break;

        case IP_STATUS:

            
            if ( strncmp(GSMstateMachine.incomingMsg, GSMstateMachine.resume_msg, 4U) == 0 )
            {
                /* Reset error count */
                GSMstateMachine.error_count = 0;
                mqtt_handler.pending_mqtt_connection = 1; // Signals that mqtt connection packet is needed
                set_tcp_connecting_state();
            }
            else if ( strstr(GSMstateMachine.incomingMsg, "ALREADY CONNECT") != NULL ) {
                /* Reset error count */
                GSMstateMachine.error_count = 0;
                set_tcp_connecting_state();
            }
            else if ( strncmp(GSMstateMachine.incomingMsg, GSMstateMachine.reject_msg, 9U) == 0 ){
                GSMstateMachine.error_count++;  // If reject message
                sendAT( GSMstateMachine.command, GSMstateMachine.command_len ); // Immediately re-send command as soon as the previous fails
                GSMstateMachine.lastChangestateTime = HAL_GetTick(); // reset this state last time changed
            }
            else
                GSMstateMachine.error_count++;      // Any other behaviour

            break;

        case TCP_CONNECTING:
            if (    strncmp( GSMstateMachine.incomingMsg, (char *)"\r\nCONNECT OK", 11U ) == 0 )
            {
                /* Reset error count */
                GSMstateMachine.error_count = 0;
                set_connect_ok_state();
            }
            else if ( strstr(GSMstateMachine.incomingMsg, "CLOSED") != NULL
                    || strstr(GSMstateMachine.incomingMsg, "CONNECT FAIL") != NULL
            ){
                GSMstateMachine.error_count++;  // If reject message
                sendAT( GSMstateMachine.command, GSMstateMachine.command_len ); // Immediately re-send command as soon as the previous fails
                GSMstateMachine.lastChangestateTime = HAL_GetTick(); // reset this state last time changed
            }
            else
                GSMstateMachine.error_count++;      // Any other behaviour

            break;

        case CONNECT_OK:
            if ( strncmp( GSMstateMachine.incomingMsg, "\r\nCLOSED", 7U ) == 0 )
                set_ip_status_state();

            break;

        case INITIAL_STATE:
            if ( strncmp(GSMstateMachine.incomingMsg, GSMstateMachine.resume_msg, 12U) == 0 ){
                /* Reset error count */
                GSMstateMachine.error_count = 0;
                set_waiting_sim_state();
            } else if ( strncmp(GSMstateMachine.incomingMsg, GSMstateMachine.reject_msg, 9U) == 0 )
                GSMstateMachine.error_count++;      // If reject message
            else
                GSMstateMachine.error_count++;      // Any other behaviour

            break;
        
        case TCP_CLOSED:
            // Do something -\_('-')_/-

            break;

        case SENDING_MSG:
            if ( strncmp( GSMstateMachine.incomingMsg, "\r\n>", 3U ) == 0 ) {

                mqtt_handler.msgBuff[ mqtt_handler.msgLen ] = 0x1A;
                sendAT( mqtt_handler.msgBuff, mqtt_handler.msgLen + 1 );

                mqtt_handler.requestForSent = 0;

            } else if ( strncmp( GSMstateMachine.incomingMsg, "\r\nSEND OK\r\n", 11U ) == 0
                        || strncmp( GSMstateMachine.incomingMsg, "\r\nOK\r\n", 6U ) == 0 ) {
                
                mqtt_handler.pending_mqtt_connection = 0;
                set_connect_ok_state(); // Go back to connection ok state

            } else if ( strncmp( GSMstateMachine.incomingMsg, "\r\nCLOSED\r\n", 10U ) == 0
                        || strncmp( GSMstateMachine.incomingMsg, "\r\nERROR\r\n", 9U ) == 0 ) {

                set_ip_status_state();
            }

        // On start (state < WAITING_SIM), the status will not be set yet.
        default:
            break;
        }
        
        // memset( GSMstateMachine.incomingMsg, '\0', sizeof(GSMstateMachine.incomingMsg) );
        HAL_UARTEx_ReceiveToIdle_IT(
            GSMstateMachine.huartAT,
            (unsigned char *) GSMstateMachine.incomingMsg,
            sizeof( GSMstateMachine.incomingMsg )
        );

    } else {
    }
}