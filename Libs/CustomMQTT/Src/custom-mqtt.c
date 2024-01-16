#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "custom-mqtt.h"
#include "custom-at.h"

extern ATmssg_t stateMachine; // Defined here but declared at file: ./main.c
extern mqt_t mqtt_handle;
int32_t timeControl = 0;

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

int createMqttConnection ( char* clientID, char* User, char*Password ) {
    
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
    
    int8_t totalMsgLen = (int8_t)( sizeof( HeaderFlag ) + (msgLen + 1) );
    
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

    memcpy( mqtt_handle.msgBuff, buff, totalMsgLen );
    mqtt_handle.totalMsgLen = totalMsgLen;
    mqtt_handle.msgBuff[totalMsgLen] = '\r';
    mqtt_handle.msgBuff[totalMsgLen + 1] = '\n';
    mqtt_handle.availableToSend = 1;

    memset( buff, 0, totalMsgLen );
    free( buff );

    return 1;
}

void sendMQTTpayload ( char * topic, char * payload ) {
    
    int8_t HeaderFlag = 0x30; //    Standard value
    int16_t topicL = strlen( topic );
    int16_t topicLen = (topicL << 8) | ( (topicL >> 8) & 0xFF );
    
    /*
        Controlls the size that should be reserved for the payload message. Note that, depending on the payload size
        the msgSize reserved on the TCP packet should be 1 or 2 bytes
    */
    int16_t msgLen = strlen( topic ) + strlen( payload ) + sizeof( topicLen );

    unsigned char encoded_msg[16];
    int msg_byte_num;
    
    encode_variable_length_integer( (int)msgLen, (unsigned char *)encoded_msg, &msg_byte_num );
    
    int16_t totalSize = sizeof( HeaderFlag ) + sizeof( msgLen ) + sizeof(topicLen) + (int)strlen(topic) + (int)strlen(payload);
    char * ptr = (char*)malloc( totalSize );
    if ( ptr == NULL ) return;
    
    ptr[0] = HeaderFlag;
    memcpy( &ptr[1], &encoded_msg, msg_byte_num );
    
    memcpy( &ptr[2 + msg_byte_num - 1], &topicLen, sizeof(topicLen) );
    memcpy( &ptr[4 + msg_byte_num - 1], topic, strlen(topic) );
    memcpy( &ptr[4 + msg_byte_num - 1 + topicL], payload, strlen(payload) );

    char * target;
    MemoryToString( ptr, totalSize, &target );
    free( target );

    memset( ptr, 0, totalSize );
    free( ptr );


}

void mqtt_init (
    char * mqtt_user,
    char * mqtt_psw,
    char * mqtt_id
) {
    mqtt_handle.availableToSend = 0;
    mqtt_handle.frequency = 1000L;

    mqtt_handle.user = mqtt_user;
    mqtt_handle.psw = mqtt_psw;
    mqtt_handle.id = mqtt_id;
}

void mqtt_task () {

    if ( HAL_GetTick() - timeControl > mqtt_handle.frequency )
    {
        //  Handle connection and re-connection
        if ( stateMachine.state == CONNECT_OK && mqtt_handle.availableToSend )
        {
            sendAT( "AT+CIPSEND\r\n", 12U );
            timeControl = HAL_GetTick();
        }
    }

}