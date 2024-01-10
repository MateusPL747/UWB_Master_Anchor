#ifndef MQTT_H
#define MQTT_H

void createMqttConnection ( char* clientID );

void sendMQTTpayload ( char * topic, char * payload );

#endif