#include "main.h"
#include "custom-at.h"
#include "custom-mqtt.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

ATmssg_t GSMstateMachine;

void resetGSM () {

    /* Pull down the PWR_KEY and RST_PIN*/
    HAL_GPIO_WritePin( GSM_RST_GPIO_Port, GSM_RST_Pin, GPIO_PIN_RESET );

    HAL_GPIO_WritePin( GSM_PWR_GPIO_Port, GSM_PWR_Pin, GPIO_PIN_RESET );

    HAL_Delay( 500 );

    /* Pull up the PWR_KEY and RST_PIN */
    HAL_GPIO_WritePin( GSM_RST_GPIO_Port, GSM_RST_Pin, GPIO_PIN_SET );

    HAL_GPIO_WritePin( GSM_PWR_GPIO_Port, GSM_PWR_Pin, GPIO_PIN_SET );

    // Pull down PWRKEY for more than 1 second according to manual requirements
    HAL_GPIO_WritePin( GSM_PWR_KEY_GPIO_Port, GSM_PWR_KEY_Pin, GPIO_PIN_SET );
    HAL_Delay( 100 );
    HAL_GPIO_WritePin( GSM_PWR_KEY_GPIO_Port, GSM_PWR_KEY_Pin, GPIO_PIN_RESET );
    HAL_Delay( 1000 );
    HAL_GPIO_WritePin( GSM_PWR_KEY_GPIO_Port, GSM_PWR_KEY_Pin, GPIO_PIN_SET );

    HAL_Delay(500);

}

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
    int pin_user,
    char * mqtt_host,
    char * mqtt_user,
    char * mqtt_psw,
    int mqtt_port)
{

    GSMstateMachine.lastChangestateTime = HAL_GetTick();
    GSMstateMachine.timeout = 5 * STATE_TIMEOUT;

    GSMstateMachine.huartAT = huart;

    GSMstateMachine.apn_host = apn_host;
    GSMstateMachine.apn_user = apn_user;
    GSMstateMachine.apn_psw = apn_psw;
    GSMstateMachine.pin = pin_user;

    GSMstateMachine.mqtt_host = mqtt_host;
    GSMstateMachine.mqtt_port = mqtt_port;

    GSMstateMachine.error_count = 0;
    GSMstateMachine.interruptFlag = 0;
    GSMstateMachine.clearAllFlag = 0;
    GSMstateMachine.modem_is_alive = 1;
    GSMstateMachine.time_to_check_modem = 0;

    GSMstateMachine.state = INITIAL_STATE; // Max value for int8_t indicating that it doesn't have any state yet

    // Start receiving UART data from modem
    HAL_UARTEx_ReceiveToIdle_IT(
        GSMstateMachine.huartAT,
        (unsigned char *) GSMstateMachine.incomingMsg,
        sizeof( GSMstateMachine.incomingMsg )
    );

    set_initial_state();

    sendAT("ATE0&W;+CFUN=1,1\r\n", 21);
}

void sendAT(char *msg, int len )
{
    HAL_UART_Transmit_IT(GSMstateMachine.huartAT, (const unsigned char *) msg, len);
}

/**
 * @brief This function aims to check periodically if the GSM connection is still online. It forces IP_STATUS state
 * that will trigger a GSM connection check. If connection is closed, it will try to reconnect until error is trigered
 * 
 */
void is_GSM_online () {
    //  Check if CIPSTATUS equals to "CONNECT OK"
    set_ip_status_state();
}

/**
 * @brief This function checks if GSM modem is responding. If not it restart modem power connection.
 * 
 */
void is_GSM_alive () {
    if ( GSMstateMachine.modem_is_alive ) {
        GSMstateMachine.modem_is_alive = 0; // Force it to 0 so it can be refreshed when a GSM Serial packet arrives
    } else {
        //  GSM not responding
        set_initial_state();
    }

    GSMstateMachine.time_to_check_modem = HAL_GetTick();
}

void GSM_task(){

    resolveUARTCtrl( GSMstateMachine.huartAT );
    clearUARTCtrl( GSMstateMachine.huartAT );

    if ( (uint8_t)GSMstateMachine.state == UNDEFINED_STATE ) return; // Do nothing when undefined state

    //  Check if there if the error count surpasses this state's limit 
    if ( is_error_count_out() ) set_error_state();

    //  If the last command timed out
    if ( !GSMstateMachine.disable_state_timeout && is_state_timeout() ) {
        GSMstateMachine.error_count++; // Since the last command didn't succeed, consider an error
        sendAT(GSMstateMachine.command, GSMstateMachine.command_len);
        GSMstateMachine.lastChangestateTime = HAL_GetTick();
    }

    //  Connection check if it has already stablished a connection before
    if ( HAL_GetTick() - GSMstateMachine.time_to_check_connection > CONNECTION_CHECK_INTERVAL // Time to check connection
            && GSMstateMachine.state > IP_STATUS && GSMstateMachine.state <= CONNECT_OK  ) {
        is_GSM_online();
    }

    // Check if any response was given from teh GSM modem from the last CONNECTION_CHECK_INTERVAL time
    if ( HAL_GetTick() > MODEM_CHECK_INTERVAL // MODEM_CHECK_INTERVAL seconds after system starts
        && HAL_GetTick() - GSMstateMachine.time_to_check_modem > MODEM_CHECK_INTERVAL ) {
        is_GSM_alive();
    }

}