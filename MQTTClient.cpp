#include "MQTTClient.h"



//------------------------------------------------------------------------------------
static const char* _MODULE_ = "[MqttCli].......";
#define _EXPR_	(ActiveModule::_defdbg)
#define MQTT_LOCAL_PUBLISHER


//------------------------------------------------------------------------------------
static Callback<esp_err_t(esp_mqtt_event_handle_t)> s_mqtt_EventHandle_cb;


//------------------------------------------------------------------------------------
static esp_err_t mqtt_EventHandler_cb(esp_mqtt_event_handle_t event)
{
    return s_mqtt_EventHandle_cb(event);
};


MQTTClient::MQTTClient(FSManager* fs, bool defdbg) : 
            ActiveModule("MqttCli", osPriorityNormal, 4096, fs, defdbg) 
{
    // Establece el soporte de JSON
	_json_supported = false;
	#if MQTT_ENABLE_JSON_SUPPORT == 1
	_json_supported = true;
	#endif

    esp_log_level_set(_MODULE_, APP_MQTTCLIENT_LOG_LEVEL);
    esp_log_level_set("MQTT_CLIENT", APP_MQTTAPI_LOG_LEVEL);
    esp_log_level_set("TRANS_TCP", APP_MQTTAPI_LOG_LEVEL);
    esp_log_level_set("OUTBOX", APP_MQTTAPI_LOG_LEVEL);

    

    msgCounter = 0;
    // Carga callbacks estáticas de publicación/suscripción
    _publicationCb = callback(this, &MQTTClient::publicationCb);
    _subscriptionCb = callback(this, &MQTTClient::subscriptionCb);

    subscriptionToServerCb = callback(this, &MQTTClient::subscrToServerCb);

    clientHandle = NULL;
    mqttCfg = {};
    _mqtt_man.cfg = {};
    _serverBridges.clear();
    topicsSubscribed.clear();
    _mqtt_man.stat.isConnected = false;

    #if defined(MQTT_LOCAL_PUBLISHER)
    myPublisher=callback(&MQ::MQClient::publish);
    #else
    myPublisher=callback(&WifiInterface::meshPublish);
    #endif

    s_mqtt_EventHandle_cb = callback(this, &MQTTClient::mqtt_EventHandler);
}

//------------------------------------------------------------------------------------
void MQTTClient::init(const char* rootTopic, const char* clientId, const char* networkId)
{
    sprintf(this->rootNetworkTopic, "%s/%s", rootTopic, networkId);
    strcpy(this->clientId, clientId);

    // utiliza el networkId para la suscripción al topic de dispositivo mqtt: cmd/dev/$(networkId)/#
	sprintf(subscTopic[0], "%s/%s/get/#", rootTopic, networkId);
    sprintf(subscTopic[1], "%s/%s/set/#", rootTopic, networkId);

    DEBUG_TRACE_D(_EXPR_, _MODULE_, "Configuracion propia del cliente MQTT:");
    DEBUG_TRACE_D(_EXPR_, _MODULE_, "Mqtt server: %s", _mqtt_man.cfg.mqttUrl);
    DEBUG_TRACE_D(_EXPR_, _MODULE_, "Mqtt port: %d", _mqtt_man.cfg.mqttPort);
    DEBUG_TRACE_D(_EXPR_, _MODULE_, "Mqtt user: %s", _mqtt_man.cfg.mqttUser);
    DEBUG_TRACE_D(_EXPR_, _MODULE_, "Mqtt password: %s", _mqtt_man.cfg.mqttPass);

    setConfigMQTTServer(_mqtt_man.cfg.mqttUrl,
         _mqtt_man.cfg.mqttPort,
         _mqtt_man.cfg.mqttUser,
         _mqtt_man.cfg.mqttPass);

    //carga la configuración y ejecuta el cliente mqtt
    clientHandle = esp_mqtt_client_init(&mqttCfg);
    esp_mqtt_client_start(clientHandle);
}

//------------------------------------------------------------------------------------
bool MQTTClient::setConfig(const char *url, uint32_t port, const char *user, const char *pass, bool restart)
{
    strcpy(_mqtt_man.cfg.mqttUrl, url);
    _mqtt_man.cfg.mqttPort = port;
    strcpy(_mqtt_man.cfg.mqttUser, user);
    strcpy(_mqtt_man.cfg.mqttPass, pass);

    saveConfig();

    if(restart){
        // espera un segundo para completar el reinicio
        DEBUG_TRACE_W(_EXPR_, _MODULE_, "#@#@#@#@#@------- REINICIANDO DISPOSITIVO -------@#@#@#@#@#");
        Thread::wait(1000);
        esp_restart();
    }

    if(!checkIntegrity()){
        DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_CFG. Ha fallado el check de integridad. Establece configuraci�n por defecto.");
        return false;
    }
    else{
        DEBUG_TRACE_D(_EXPR_, _MODULE_, "Check de integridad OK.");
        return true;
    }
}

//------------------------------------------------------------------------------------
void MQTTClient::setConfigMQTTServer(const char *host, uint32_t port, const char *user, const char *pass)
{
    mqttCfg.host = host;
    mqttCfg.port = port;
    mqttCfg.username = user;
    mqttCfg.password = pass;
    mqttCfg.keepalive = 30;
    mqttCfg.event_handle = mqtt_EventHandler_cb;

    sprintf(last_will_topic, "%s/stat/0/%s/last_will/%s", this->rootNetworkTopic, this->clientId, _sub_topic_base);
    mqttCfg.lwt_topic = last_will_topic;
    mqttCfg.lwt_msg = "{}";
    mqttCfg.lwt_msg_len = strlen(mqttCfg.lwt_msg);
    mqttCfg.lwt_qos = 0;
    mqttCfg.lwt_retain  = 0;
}



//------------------------------------------------------------------------------------
void MQTTClient::publicationCb(const char* topic, int32_t result)
{
	DEBUG_TRACE_D(_EXPR_, _MODULE_, "PUB_DONE en topic local '%s' con resultado '%d'", topic, result);
}


//------------------------------------------------------------------------------------
esp_err_t MQTTClient::mqtt_EventHandler(esp_mqtt_event_handle_t event)
{
    // crea el mensaje para publicar en la m�quina de estados
    State::Msg* op = (State::Msg*)Heap::memAlloc(sizeof(State::Msg));
    MBED_ASSERT(op);
    // el mensaje a publicar es un blob tipo MqttEvtMsg_t
	MqttEvtMsg_t* mdata = (MqttEvtMsg_t*)Heap::memAlloc(sizeof(MqttEvtMsg_t));
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
            _mqtt_man.stat.isConnected = true;
            ev = MqttConnEvt;
            break;

        case MQTT_EVENT_DISCONNECTED:
            DEBUG_TRACE_D(_EXPR_, _MODULE_, "MQTT_EVENT_DISCONNECTED");
            ev = MqttDiscEvt;
            _mqtt_man.stat.isConnected = false;
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
            MqttTopicData_t* mqttData = (MqttTopicData_t*)Heap::memAlloc(sizeof(MqttTopicData_t));
            MBED_ASSERT(mqttData);

            MBED_ASSERT((int) event->topic_len <= Blob::MaxLengthOfMqttStrings);
	    	strncpy(mqttData->topic, event->topic, (int) event->topic_len);
	    	mqttData->topic[event->topic_len] = 0;

            if(!_json_supported)
            {
                mqttData->data = JsonParser::getObjFromDataTopic(mqttData->topic, event->data, &mqttData->data_len);
                //JsonParser::printBinaryObject(mqttData->topic, mqttData->data, mqttData->data_len);
            }
            else
                mqttData->data = NULL;

            if(mqttData->data == NULL)
            {
                DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_MSG. Error parsing msg, topic [%s]. Enviando mensaje en formato JSON", (char*)mqttData->topic);
                mqttData->data_len = event->data_len + 1;
                mqttData->data = (char*)Heap::memAlloc(mqttData->data_len);
                MBED_ASSERT(mqttData->data);
                memcpy(mqttData->data, event->data, event->data_len);
                ((char*)mqttData->data)[event->data_len] = 0;
            }
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
    	DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_PUT mqtt_EventHandler");
        if(ev == MqttDataEvt){
    		MqttTopicData_t* topicdata = (MqttTopicData_t*)mdata->data;
    		Heap::memFree(topicdata->data);
    		Heap::memFree(topicdata);
    	}
    	if(op->msg){
    		Heap::memFree(op->msg);
    	}
    	Heap::memFree(op);
    }
    return ESP_OK;
}


//------------------------------------------------------------------------------------
void MQTTClient::notifyConnStatUpdate()
{   
    char* pub_topic = (char*)Heap::memAlloc(MQ::MQClient::getMaxTopicLen());
    MBED_ASSERT(pub_topic);
    sprintf(pub_topic, "stat/conn/%s", _pub_topic_base);
    
    if(_json_supported){
        cJSON* jStat = JsonParser::getJsonFromObj(_mqtt_man.stat.connStatus);
        MBED_ASSERT(jStat);

        MQ::MQClient::publish(pub_topic, &jStat, sizeof(cJSON**), &_publicationCb);
        cJSON_Delete(jStat);
    }
    else{
        MQ::MQClient::publish(pub_topic, &_mqtt_man.stat.connStatus, sizeof(Blob::MqttStatusFlags), &_publicationCb);
    }
    Heap::memFree(pub_topic);
    DEBUG_TRACE_I(_EXPR_, _MODULE_, "MQTT en estado = %x", (uint32_t)_mqtt_man.stat.connStatus);
}


//------------------------------------------------------------------------------------
void MQTTClient::parseMqttTopic(char* localTopic, const char* mqttTopic)
{
	// por defecto marco resultado como incorrecto
	localTopic[0] = 0;

    if(MQ::MQClient::isTokenRoot(mqttTopic, rootNetworkTopic))
        strcpy(localTopic, &mqttTopic[strlen(rootNetworkTopic)+1]);

	DEBUG_TRACE_D(_EXPR_, _MODULE_, "Conversion de topic mqtt '%s' a topic local '%s'", mqttTopic, localTopic);
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
    char* topic = (char*)Heap::memAlloc(strlen(localTopic)+1);
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

    Heap::memFree(topic);

    return isCorrect;
}


//------------------------------------------------------------------------------------
int32_t MQTTClient::publish(const char* topic, void *data, uint32_t dataSize,
    Callback<void(const char*, int32_t)> *publisher)
{
    if(_json_supported)
    {
        cJSON* json = cJSON_Parse(data);
        int32_t res = myPublisher(topic, &json, sizeof(cJSON**), publisher);
        cJSON_Delete(json);
        return res;
    }
    else
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

    char* ref1;
    char* ref2;
    char* copyBridge;
    char* copyTopic;
    char* subBridge;
    char* subTopic;

    for(auto &bridge : _serverBridges)
    {
        
        ref1 = strdup(bridge);
        ref2 = strdup(topic);
        copyBridge = ref1;
        copyTopic = ref2;
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

        Heap::memFree(ref1);
        Heap::memFree(ref2);

        if(isBridge)
            break;
            
    }
    
    return isBridge;
}



void MQTTClient::stop(){
    esp_mqtt_client_stop(clientHandle);
}

void MQTTClient::start(){
    esp_mqtt_client_start(clientHandle);
}


void MQTTClient::sendPing(){
    char* pub_topic = (char*)Heap::memAlloc(MQ::MQClient::getMaxTopicLen());
    MBED_ASSERT(pub_topic);
    sprintf(pub_topic, "stat/ping/%s", _pub_topic_base);
    
    if(_json_supported){
        cJSON* jStat = cJSON_CreateObject();
        MBED_ASSERT(jStat);

        MQ::MQClient::publish(pub_topic, &jStat, sizeof(cJSON**), &_publicationCb);
        cJSON_Delete(jStat);
    }
    else{
        MQ::MQClient::publish(pub_topic, "{}", strlen("{}")+1, &_publicationCb);
    }
    Heap::memFree(pub_topic);
    DEBUG_TRACE_I(_EXPR_, _MODULE_, "MQTT ping");
}