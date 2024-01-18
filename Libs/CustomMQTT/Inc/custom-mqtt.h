int encode_variable_length_integer(int value, unsigned char *encoded_bytes, int *num_bytes);

int sendMQTTpayload ( char * topic, char * payload );
int createMqttConnection ( char* clientID, char* User, char*Password );

void MemoryToString( const void *buffer, size_t length, char **hexString );

void mqtt_init ( char * mqtt_user, char * mqtt_psw, char * mqtt_id );

void mqtt_task ();

typedef struct {
    int availableToSend;
    char msgBuff[800];
    size_t totalMsgLen;
    char * id;
    char * user;
    char * psw;
    int32_t frequency;
    int32_t connected;

} mqt_t;