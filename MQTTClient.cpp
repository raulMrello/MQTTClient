#include "MQTTClient.h"



//------------------------------------------------------------------------------------
static const char* _MODULE_ = "[MqttCli].......";
#define _EXPR_	(ActiveModule::_defdbg)
#define MQTT_LOCAL_PUBLISHER


//------------------------------------------------------------------------------------
mwifi_data_type_t data_type      = {0x0};
static Callback<esp_err_t(esp_mqtt_event_handle_t)> s_mqtt_EventHandle_cb;


//------------------------------------------------------------------------------------
static esp_err_t mqtt_EventHandler_cb(esp_mqtt_event_handle_t event)
{
    return s_mqtt_EventHandle_cb(event);
};


//------------------------------------------------------------------------------------
MQTTClient::MQTTClient(const char* rootTopic, const char* clientId, const char* networkId,
            const char *uri, FSManager* fs, bool defdbg) : 
            ActiveModule("MqttCli", osPriorityNormal, 4096, fs, defdbg) 
{

	// Establece el soporte de JSON
	_json_supported = false;
	#if MQTT_ENABLE_JSON_SUPPORT == 1
	_json_supported = true;
	#endif

    if(defdbg){
    	esp_log_level_set(_MODULE_, ESP_LOG_DEBUG);
    }
    else{
    	esp_log_level_set(_MODULE_, ESP_LOG_WARN);
    }


    init(rootTopic, clientId, networkId);
    setConfigMQTTServer(uri);
}


//------------------------------------------------------------------------------------
MQTTClient::MQTTClient(const char* rootTopic, const char* clientId, const char* networkId,
            const char *host, uint32_t port, FSManager* fs, bool defdbg) : 
            ActiveModule("MqttCli", osPriorityNormal, 4096, fs, defdbg) 
{

	// Establece el soporte de JSON
	_json_supported = false;
	#if MQTT_ENABLE_JSON_SUPPORT == 1
	_json_supported = true;
	#endif

	esp_log_level_t level;
    if(defdbg){
    	level = ESP_LOG_DEBUG;
    }
    else{
    	level = ESP_LOG_WARN;
    }
    esp_log_level_set(_MODULE_, level);
    esp_log_level_set("MQTT_CLIENT", level);
    esp_log_level_set("TRANS_TCP", level);


    init(rootTopic, clientId, networkId);
    setConfigMQTTServer(host, port);
}


//------------------------------------------------------------------------------------
void MQTTClient::init(const char* rootTopic, const char* clientId, const char* networkId)
{
    sprintf(this->rootNetworkTopic, "%s/%s", rootTopic, networkId);
    strcpy(this->clientId, clientId);

    // utiliza el networkId para la suscripción al topic de dispositivo mqtt: cmd/dev/$(networkId)/#
	sprintf(subscTopic[0], "%s/%s/get/#", rootTopic, networkId);
    sprintf(subscTopic[1], "%s/%s/set/#", rootTopic, networkId);

    msgCounter = 0;
    // Carga callbacks estáticas de publicación/suscripción
    _publicationCb = callback(this, &MQTTClient::publicationCb);
    _subscriptionCb = callback(this, &MQTTClient::subscriptionCb);

    subscriptionToServerCb = callback(this, &MQTTClient::subscrToServerCb);

    clientHandle = NULL;
    mqttCfg = {};
    mqttLocalCfg = {};
    _serverBridges.clear();
    topicsSubscribed.clear();
    isConnected = false;

    #if defined(MQTT_LOCAL_PUBLISHER)
    myPublisher=callback(&MQ::MQClient::publish);
    #else
    myPublisher=callback(&WifiInterface::meshPublish);
    #endif

    s_mqtt_EventHandle_cb = callback(this, &MQTTClient::mqtt_EventHandler);
}


//------------------------------------------------------------------------------------
void MQTTClient::setConfigMQTTServer(const char *uri)
{
    mqttCfg.uri = uri;
    mqttCfg.event_handle = mqtt_EventHandler_cb;
}


//------------------------------------------------------------------------------------
void MQTTClient::setConfigMQTTServer(const char *host, uint32_t port)
{
    mqttCfg.host = host;
    mqttCfg.port = port;
    mqttCfg.event_handle = mqtt_EventHandler_cb;
}


//------------------------------------------------------------------------------------
osStatus MQTTClient::putMessage(State::Msg *msg){
    osStatus ost = queueSM.put(msg, ActiveModule::DefaultPutTimeout);
    if(ost != osOK){
        DEBUG_TRACE_E(_EXPR_, _MODULE_, "QUEUE_PUT_ERROR %d", ost);
    }
    return ost;
}


//------------------------------------------------------------------------------------
void MQTTClient::publicationCb(const char* topic, int32_t result)
{
	DEBUG_TRACE_I(_EXPR_, _MODULE_, "PUB_DONE en topic local '%s' con resultado '%d'", topic, result);
}


//------------------------------------------------------------------------------------
esp_err_t MQTTClient::mqtt_EventHandler(esp_mqtt_event_handle_t event)
{
    // crea el mensaje para publicar en la m�quina de estados
    State::Msg* op = (State::Msg*)malloc(sizeof(State::Msg));
    MBED_ASSERT(op);
    // el mensaje a publicar es un blob tipo MqttEvtMsg_t
	MqttEvtMsg_t* mdata = (MqttEvtMsg_t*)malloc(sizeof(MqttEvtMsg_t));
    MBED_ASSERT(mdata);
    mdata->data = NULL;
    int ev = MqttErrorEvt;

    clientHandle = event->client;
    //int msgId;
    // your_context_t *context = event->context;
    switch (event->event_id) 
    {
        case MQTT_EVENT_CONNECTED:
            DEBUG_TRACE_D(_EXPR_, _MODULE_, "MQTT_EVENT_CONNECTED");
            isConnected = true;
            ev = MqttConnEvt;
            break;

        case MQTT_EVENT_DISCONNECTED:
            DEBUG_TRACE_D(_EXPR_, _MODULE_, "MQTT_EVENT_DISCONNECTED");
            isConnected = false;
            break;

        case MQTT_EVENT_SUBSCRIBED:
            DEBUG_TRACE_D(_EXPR_, _MODULE_, "MQTT_EVENT_SUBSCRIBED, msgId=%d", event->msg_id);
            topicsSubscribed[event->msg_id]=true;
            ev = MqttSubscrEvt;
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            DEBUG_TRACE_D(_EXPR_, _MODULE_, "MQTT_EVENT_UNSUBSCRIBED");
            break;
        case MQTT_EVENT_PUBLISHED:
            DEBUG_TRACE_D(_EXPR_, _MODULE_, "MQTT_EVENT_PUBLISHED");
            break;
        case MQTT_EVENT_DATA:
        {
            DEBUG_TRACE_D(_EXPR_, _MODULE_, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            ev = MqttDataEvt;
            MqttTopicData_t* mqttData = (MqttTopicData_t*)malloc(sizeof(MqttTopicData_t));
            MBED_ASSERT(mqttData);
            mqttData->data_len = event->data_len + 1;
            mqttData->data = (char*)malloc(mqttData->data_len);
	    	MBED_ASSERT(mqttData->data);
	    	memcpy(mqttData->data, event->data, event->data_len);
	    	mqttData->data[event->data_len] = 0;
	    	MBED_ASSERT((int) event->topic_len <= Blob::MaxLengthOfMqttStrings);
	    	strncpy(mqttData->topic, event->topic, (int) event->topic_len);
	    	mqttData->topic[event->topic_len] = 0;
            mdata->data = mqttData;
            break;
        }
        case MQTT_EVENT_ERROR:
            DEBUG_TRACE_D(_EXPR_, _MODULE_, "Error");
            ev = MqttErrorEvt;
            break;
        default:
            DEBUG_TRACE_D(_EXPR_, _MODULE_, "Other event id:%d", event->event_id);
            //ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }

    op->sig = ev;
    // apunta a los datos
    op->msg = mdata;

    // postea en la cola de la m�quina de estados
    if(putMessage(op) != osOK){
        if(ev == MqttDataEvt){
    		MqttTopicData_t* topicdata = (MqttTopicData_t*)mdata->data;
    		free(topicdata->data);
    		free(topicdata);
    	}
    	if(op->msg){
    		free(op->msg);
    	}
    	free(op);
    }
    return ESP_OK;
}


//------------------------------------------------------------------------------------
void MQTTClient::notifyConnStatUpdate()
{
	Blob::NotificationData_t<Blob::MqttStatusFlags> *notif = new Blob::NotificationData_t<Blob::MqttStatusFlags>(connStatus);
	MBED_ASSERT(notif);
    cJSON* jStat = JsonParser::getJsonFromNotification(*notif);
    if(jStat){
    	char* pub_topic = (char*)malloc(MQ::MQClient::getMaxTopicLen());
		MBED_ASSERT(pub_topic);
		sprintf(pub_topic, "stat/conn/%s", _pub_topic_base);
    	if(_json_supported){
			char* jmsg = cJSON_Print(jStat);
			MBED_ASSERT(jmsg);
			cJSON_Delete(jStat);
			DEBUG_TRACE_D(_EXPR_, _MODULE_, "Notificando cambio de estado flags=%s", jmsg);
			MQ::MQClient::publish(pub_topic, jmsg, strlen(jmsg)+1, &_publicationCb);
			free(jmsg);
    	}
    	else{
    		MQ::MQClient::publish(pub_topic, notif, sizeof(Blob::NotificationData_t<Blob::MqttStatusFlags>), &_publicationCb);
    	}
    	free(pub_topic);
    }
    delete(notif);
}


//------------------------------------------------------------------------------------
void MQTTClient::parseMqttTopic(char* localTopic, const char* mqttTopic)
{
	// por defecto marco resultado como incorrecto
	localTopic[0] = 0;

    if(MQ::MQClient::isTokenRoot(mqttTopic, rootNetworkTopic))
        strcpy(localTopic, &mqttTopic[strlen(rootNetworkTopic)+1]);

	DEBUG_TRACE_D(_EXPR_, _MODULE_, "Conversión de topic mqtt '%s' a topic local '%s'", mqttTopic, localTopic);
}


//------------------------------------------------------------------------------------
void MQTTClient::parseLocalTopic(char* mqtt_topic, const char* local_topic){
	// por defecto dejo el topic como inv�lido strlen(mqtt_topic) = 0
	mqtt_topic[0] = 0;

	if (MQ::MQClient::isTokenRoot(local_topic, "stat/")){
		sprintf(mqtt_topic, "%s/%s", rootNetworkTopic, local_topic);
	}
	DEBUG_TRACE_D(_EXPR_, _MODULE_, "Conversi�n de topic local mesh '%s' a topic mqtt server '%s'", local_topic, mqtt_topic);
}


//------------------------------------------------------------------------------------
bool MQTTClient::getRelativeTopic(char* relativeTopic, const char* localTopic)
{
    return getRelativeTopic(relativeTopic, localTopic, NULL);
}


//------------------------------------------------------------------------------------
bool MQTTClient::getRelativeTopic(char* relativeTopic, const char* localTopic, bool* isOwn)
{
    //Quitamos los parametros de grup y device (el segundo y el tercero)
    char* topic = (char*)malloc(strlen(localTopic)+1);
    strcpy(topic, localTopic);
    char *command = strtok(topic, "/");
    bool isCorrect = true;

    sprintf(relativeTopic, "%s/", command);

    if(command != NULL)
    {
        char *grp = strtok(NULL, "/");
        if(grp != NULL)
        {
            char *id = strtok(NULL, "/");
            if(id != NULL)
            {
                char * grpCheck = NULL;
                long int grpInt;
                grpInt = strtol(grp, &grpCheck, 10);
                //Si el grupo es 0 y el id es del nodo, significa que es un mensaje propio
                if(grpInt == 0 && strlen(grpCheck) == 0 &&
                    strcmp(id, clientId) == 0 && isOwn != NULL)
                {
                    *isOwn = true;
                }

                command = strtok(NULL, "");
                if(command != NULL)
                    strcat(relativeTopic, command);
                else
                    isCorrect = false;
            }
            else
                isCorrect = false;
        }
        else
            isCorrect = false;
    }
    else
        isCorrect = false;

    free(topic);

    return isCorrect;
}


//------------------------------------------------------------------------------------
int32_t MQTTClient::publish(const char* topic, void *data, uint32_t dataSize,
    Callback<void(const char*, int32_t)> *publisher)
{
    return myPublisher(topic, data, dataSize, publisher);
}


//------------------------------------------------------------------------------------
void MQTTClient::addServerBridge(char* topic)
{
    _serverBridges.push_back(topic);
}


//------------------------------------------------------------------------------------
void MQTTClient::removeServerBridge(char* topic)
{
    _serverBridges.erase(std::remove(_serverBridges.begin(),
         _serverBridges.end(), topic), _serverBridges.end());
}


//------------------------------------------------------------------------------------
bool MQTTClient::checkServerBridge(char* topic)
{
    bool isBridge = true;

    char* copyBridge;
    char* copyTopic;
    char* subBridge;
    char* subTopic;

    for(auto &bridge : _serverBridges)
    {
        copyBridge = strdup(bridge);
        copyTopic = strdup(topic);
        subBridge = strtok_r(copyBridge, "/", &copyBridge);
        subTopic = strtok_r(copyTopic, "/", &copyTopic);
        isBridge = true;
        
        while(subBridge != NULL && subTopic != NULL && isBridge)
        {
            if(strcmp(subBridge, "#") == 0)
            {
                subBridge = NULL;
                subTopic = NULL;
            }
            else
            {
                if(strcmp(subBridge, subTopic) != 0 && strcmp(subBridge, "+") != 0)
                    isBridge = false;
                subBridge = strtok_r(copyBridge, "/", &copyBridge);
                subTopic = strtok_r(copyTopic, "/", &copyTopic);
            }
        }

        if(isBridge)
            break;
    }

    free(copyBridge);
    free(copyTopic);
    
    return isBridge;
}


//------------------------------------------------------------------------------------
osEvent MQTTClient::getOsEvent()
{
    return queueSM.get();
}


