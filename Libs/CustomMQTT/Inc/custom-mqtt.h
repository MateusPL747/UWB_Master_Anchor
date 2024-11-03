#include <stdint.h>

int encode_variable_length_integer(int value, unsigned char *encoded_bytes, int *num_bytes);

void createMQTTpayloadPacket ( char * topic );
int createMqttConnectionPacket ( char* clientID, char* User, char*Password );

void MemoryToString( const void *buffer, size_t length, char **hexString );

void mqtt_init ( char * mqtt_user, char * mqtt_psw, char * mqtt_id );

void check_for_sending_message_timeout();

void sendPendingMessages ();

void mqtt_task ();


typedef struct {
    int availableToSend;
    int requestForSent;
    int pending_mqtt_connection;
    int send_feedback;
    char msgBuff[61];
    int msgLen;

    char * id;
    char * user;
    char * psw;

} mqt_t;