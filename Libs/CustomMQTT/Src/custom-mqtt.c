#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "custom-mqtt.h"
#include "custom-at.h"

mqt_t mqtt_handler;
extern ATmssg_t GSMstateMachine; // Declared at custom-at.c

int encode_variable_length_integer(int value, unsigned char *encoded_bytes, int *num_bytes) {
    // Check if the value is negative
    if (value < 0) {
        return 0;
    }

    // Initialize variables
    *num_bytes = 0;

    // Continue encoding until the value is reduced to 0
    while (value > 0) {
        // Take the least significant 7 bits of the value
        unsigned char byte = value & 0x7F;

        // Shift the value to the right by 7 bits
        value >>= 7;

        // Set the most significant bit to 1 if there are more bytes to follow
        if (value > 0) {
            byte |= 0x80;
        }

        // Store the byte in the encoded_bytes array
        encoded_bytes[(*num_bytes)++] = byte;
    }

    return 1;
}

void MemoryToString( const void *buffer, size_t length, char **hexString ) {
    if (buffer == NULL || length == 0 || hexString == NULL) {
        // Handle invalid input
        return;
    }

    const unsigned char *data = (const unsigned char *)buffer;
    size_t hexStringLength = length * 3 + 1; // Two characters per byte plus null terminator
    *hexString = (char *)malloc(hexStringLength);

    if (*hexString == NULL) {
        // Handle memory allocation failure
        return;
    }


    for (size_t i = 0; i < length; ++i) {
        sprintf(*hexString + i * 3, "%02X ", data[i]);
    }

    // Add null terminator
    (*hexString)[hexStringLength - 1] = '\0';
}

int createMqttConnectionPacket ( char* clientID, char* User, char*Password ) {
    
    int16_t ProtocolNameLen = (0x04 << 8) | ((0x04>>8) & 0xFF);  //Standard value reverse endianess
    int16_t IDlen = strlen(clientID);
    int16_t clientIDLen = ((IDlen) << 8) | (((IDlen) >> 8) & 0xFF); //reverse endianess
    int16_t userLen = 0;
    int16_t userOffset = 0;
    int16_t passwordLen = 0;

    if ( User != NULL ) {
        userLen = strlen( User );
        userOffset = userLen;
        userLen = (userLen << 8) | ((userLen >> 8) & 0xFF); //reverse endianess
    }

    if ( User != NULL ) {
        passwordLen = strlen( Password );
        passwordLen = (passwordLen << 8) | ((passwordLen >> 8) & 0xFF); //reverse endianess
    }
    
    
    int8_t HeaderFlag = 0x10;       //Standard value
    char ProtocolName[] = "MQTT";
    int8_t ProtocolVer = 0x04;
    int8_t connectFlags = 0x02;
    int16_t keepAlive = ( 60<<8 ) | (( 60>>8 )& 0xFF);
    
    int8_t msgLen = 0;
    if ( User == NULL && Password == NULL) {
        connectFlags = 0x02;
        msgLen =
            strlen( clientID )
            + sizeof(clientIDLen)
            + strlen( ProtocolName )
            + sizeof(ProtocolNameLen)
            + sizeof( ProtocolVer )
            + sizeof( connectFlags )
            + sizeof( keepAlive );

    } else if ( User != NULL && Password != NULL) {
        connectFlags = 0xC2;
        msgLen =
            strlen( clientID )
            + sizeof(clientIDLen)
            + strlen( ProtocolName )
            + sizeof(ProtocolNameLen)
            + sizeof( ProtocolVer )
            + sizeof( connectFlags )
            + sizeof( keepAlive )
            + strlen( User )
            + sizeof( userLen )
            + strlen( Password )
            + sizeof( passwordLen );

    } else if ( Password == NULL ) {
        connectFlags = 0x82;
        msgLen =
            strlen( clientID )
            + sizeof(clientIDLen)
            + strlen( ProtocolName )
            + sizeof(ProtocolNameLen)
            + sizeof( ProtocolVer )
            + sizeof( connectFlags )
            + sizeof( keepAlive )
            + strlen( User )
            + sizeof( userLen );
            
    }
    
    int8_t totalMsgLen = (int8_t)( sizeof( HeaderFlag ) + msgLen );
    
    char * buff = (char*)malloc(totalMsgLen);
    if ( buff == NULL ) {
        return 0;
    }
    
    buff[0] = HeaderFlag;
    buff[1] = msgLen;
    buff[8] = ProtocolVer;
    buff[9] = connectFlags;
    
    memcpy( &buff[2], &ProtocolNameLen, sizeof(ProtocolNameLen) );
    memcpy( &buff[4], ProtocolName, strlen(ProtocolName) );
    memcpy( &buff[10], &keepAlive, sizeof( keepAlive ) );
    memcpy( &buff[12], &clientIDLen, sizeof( clientIDLen ) );
    memcpy( &buff[14], clientID, strlen( clientID ) );
    
    if ( User != NULL ) {
        memcpy( &buff[14 + IDlen ], &userLen, sizeof( userLen ) );
        memcpy( &buff[14 + IDlen + sizeof(userLen)], User, strlen( User ) );
    }

    if ( Password != NULL ) {
        memcpy( &buff[14 + IDlen + sizeof(userLen) + userOffset], &passwordLen, sizeof( passwordLen ) );
        memcpy( &buff[14 + IDlen + sizeof(userLen) + userOffset + sizeof(passwordLen) ], Password, strlen( Password ) );
    }
    
    memcpy( mqtt_handler.msgBuff, buff, totalMsgLen + 1 );
    mqtt_handler.msgLen = totalMsgLen + 1;

    memset( buff, 0, totalMsgLen ); //  Sets 0 to all buffer
    free( buff );

    return 1;
}

void createMQTTpayloadPacket ( char * topic ) {
    
    int8_t HeaderFlag = 0x30; //    Standard value
    int16_t topicL = strlen( topic );
    int16_t topicLen = (topicL << 8) | ( (topicL >> 8) & 0xFF );
    
    /*
        Controlls the size that should be reserved for the payload message. Note that, depending on the payload size
        the msgSize reserved on the TCP packet should be 1 or 2 bytes
    */
    int8_t msgLen = strlen( topic ) + mqtt_handler.msgLen + sizeof( topicLen );

    unsigned char encoded_msg[16];
    int msg_byte_num;
    
    encode_variable_length_integer( (int)msgLen, (unsigned char *)encoded_msg, &msg_byte_num );
    
    int16_t totalSize = sizeof( HeaderFlag ) + sizeof( msgLen ) + sizeof(topicLen) + (int)strlen(topic) + mqtt_handler.msgLen;
    char * ptr = (char*)malloc( totalSize );
    if ( ptr == NULL ) return;
    
    ptr[0] = HeaderFlag;
    memcpy( &ptr[1], &encoded_msg, msg_byte_num );
    
    memcpy( &ptr[2 + msg_byte_num - 1], &topicLen, sizeof(topicLen) );
    memcpy( &ptr[4 + msg_byte_num - 1], topic, strlen(topic) );
    memcpy( &ptr[4 + msg_byte_num - 1 + topicL], mqtt_handler.msgBuff, mqtt_handler.msgLen );

    memcpy( mqtt_handler.msgBuff, ptr, totalSize );
    mqtt_handler.msgLen = totalSize;

    memset( ptr, 0, totalSize );
    free( ptr );

}

void mqtt_init (
    char * mqtt_user,
    char * mqtt_psw,
    char * mqtt_id
) {
    mqtt_handler.availableToSend = 0;
    mqtt_handler.requestForSent = 0;

    mqtt_handler.user = mqtt_user;
    mqtt_handler.psw = mqtt_psw;
    mqtt_handler.id = mqtt_id;
}

void connectToMQTTBroker () {
    
    /* Here, the mqtt connect packet will be created and the mqtt_handle.availableToSend will be set to 1 for the first time */
    createMqttConnectionPacket( mqtt_handler.id, mqtt_handler.user, mqtt_handler.psw );

    set_sending_msg_state(); //After set up packet, set gsm state machine as SENDING_MSG
    sendAT( "AT+CIPSEND\r\n", 12U );
}

void sendPendingMessages() {

    /* Here, the payload packet will be created */
    createMQTTpayloadPacket((char *)"UWB_test/teste1" );

    set_sending_msg_state(); // After set up packet, set gsm state machine as SENDING_MSG
    sendAT( "AT+CIPSEND\r\n", 12U );
}

void check_for_sending_message_timeout () {
    if ( GSMstateMachine.state != SENDING_MSG ) return;

    if ( HAL_GetTick() - GSMstateMachine.lastChangestateTime > 5000 ) {;
        set_initial_state();
    }
}

int32_t lastSendMoment = 0;
void mqtt_task () {

    check_for_sending_message_timeout();

    if ( GSMstateMachine.state != CONNECT_OK ) return; // This task only works when GPRS is connected

    if ( mqtt_handler.pending_mqtt_connection ) connectToMQTTBroker();

    if ( !mqtt_handler.pending_mqtt_connection && mqtt_handler.requestForSent ) sendPendingMessages();


}