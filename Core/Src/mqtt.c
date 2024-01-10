#include <mqtt.h>

/**
 * @brief create an MQTT connatcion packet
 * 
 * @param   clientID 
 *          char* clientID - The desired client ID
 * 
 */
void createMqttConnection ( char* clientID ) {
    
    int8_t msgLen;
    int16_t ProtocolNameLen = (0x04 << 8) | ((0x04>>8) & 0xFF);  //Standard value reverse endianess
    int16_t IDlen = strlen(clientID);
    int16_t clientIDLen = ((IDlen) << 8) | (((IDlen) >> 8) & 0xFF); //reverse endianess
    
    
    int8_t HeaderFlag = 0x10;       //Standard value
    char ProtocolName[] = "MQTT";
    int8_t ProtocolVer = 0x04;
    int8_t connectFlags = 0x02;
    int16_t keepAlive = ( 60<<8 ) | (( 60>>8 )& 0xFF);
    
    msgLen = sizeof(clientIDLen) + strlen( ProtocolName ) + sizeof(ProtocolNameLen) + sizeof( ProtocolVer ) +  sizeof( connectFlags ) + sizeof( keepAlive ) + strlen( clientID );
    
    int8_t totalMsgLen = (int8_t)( sizeof( HeaderFlag ) + msgLen );
    
    char * ptr = (char*)malloc(totalMsgLen);
    if ( ptr == NULL ) {
        printf("void");
    }
    
    ptr[0] = HeaderFlag;
    ptr[1] = msgLen;
    ptr[8] = ProtocolVer;
    ptr[9] = connectFlags;
    
    memcpy( &ptr[2], &ProtocolNameLen, sizeof(ProtocolNameLen) );
    memcpy( &ptr[4], ProtocolName, strlen(ProtocolName) );
    memcpy( &ptr[10], &keepAlive, sizeof( keepAlive ) );
    memcpy( &ptr[12], &clientIDLen, sizeof( clientIDLen ) );
    memcpy( &ptr[14], clientID, strlen( clientID ) );

    // Execute the ST command to send data

    free( ptr );
}


/**
 * @brief Send specific payload into specific topic
 * 
 * @param topic 
 * @param payload 
 */
void sendMQTTpayload ( char * topic, char * payload ) {
    
    int8_t HeaderFlag = 0x30; //    Standard value
    int8_t msgLen;
    int16_t topicL = strlen( topic );
    int16_t topicLen = (topicL << 8) | ( (topicL >> 8) & 0xFF );
    
    msgLen = strlen( topic ) + strlen( payload ) + sizeof( topicLen );
    
    char* ptr = (char*)malloc( sizeof( HeaderFlag ) + sizeof( msgLen ) + sizeof(topicLen) + (int)strlen(topic) + (int)strlen(payload) );
    
    ptr[0] = HeaderFlag;
    ptr[1] = msgLen;
    
    memcpy( &ptr[2], &topicLen, sizeof(topicLen) );
    memcpy( &ptr[4], topic, strlen(topic) );
    memcpy( &ptr[4 + topicL], payload, strlen(payload) );
    
    for ( int i = 0; i < msgLen + 2; i++ ) {
        printf("%02X ", ptr[i]);
    }
    
    free( ptr );
}