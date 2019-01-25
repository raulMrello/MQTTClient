#include "MQTTClient.h"

/** Macro para imprimir trazas de depuraci�n, siempre que se haya configurado un objeto
 *	Logger v�lido (ej: _debug)
 */
static const char* _MODULE_ = "[MqttCli].......";
#define _EXPR_	(ActiveModule::_defdbg)

mwifi_data_type_t data_type      = {0x0};

static Callback<esp_err_t(esp_mqtt_event_handle_t)> s_mqtt_EventHandle_cb; 

static esp_err_t mqtt_EventHandler_cb(esp_mqtt_event_handle_t event)
{
    return s_mqtt_EventHandle_cb(event);
};

MQTTClient::MQTTClient(const char* rootTopic, const char* networkId,
            const char *uri, FSManager* fs, bool defdbg) : 
            ActiveModule("MqttCli", osPriorityNormal, 4096, fs, defdbg) 
{
    init(rootTopic, networkId);
    setConfigMQTTServer(uri);
}

MQTTClient::MQTTClient(const char* rootTopic, const char* networkId,
            const char *host, uint32_t port, FSManager* fs, bool defdbg) : 
            ActiveModule("MqttCli", osPriorityNormal, 4096, fs, defdbg) 
{
    init(rootTopic, networkId);
    setConfigMQTTServer(host, port);
}

void MQTTClient::init(const char* rootTopic, const char* networkId)
{
    sprintf(this->rootNetworkTopic, "%s/%s", rootTopic, networkId);

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
    topicsSubscribed.clear();
    statusFlag = 0;
    isConnected = false;

    #if defined(MQTT_LOCAL_PUBLISHER)
    myPublisher=callback(&MQ::MQClient::publish);
    #else
    myPublisher=callback(this, &MQTTClient::meshPublish);
    #endif

    s_mqtt_EventHandle_cb = callback(this, &MQTTClient::mqtt_EventHandler);
}

void MQTTClient::setConfigMQTTServer(const char *uri)
{
    mqttCfg.uri = uri;
    mqttCfg.event_handle = mqtt_EventHandler_cb;
}

void MQTTClient::setConfigMQTTServer(const char *host, uint32_t port)
{
    mqttCfg.host = host;
    mqttCfg.port = port;
    mqttCfg.event_handle = mqtt_EventHandler_cb;
}

osStatus MQTTClient::putMessage(State::Msg *msg){
    osStatus ost = queueSM.put(msg, ActiveModule::DefaultPutTimeout);
    if(ost != osOK){
        DEBUG_TRACE_E(_EXPR_, _MODULE_, "QUEUE_PUT_ERROR %d", ost);
    }
    return ost;
}

void MQTTClient::publicationCb(const char* topic, int32_t result)
{
	DEBUG_TRACE_I(_EXPR_, _MODULE_, "PUB_DONE en topic local '%s' con resultado '%d'", topic, result);
}

void MQTTClient::subscriptionCb(const char* topic, void* msg, uint16_t msg_len)
{
    DEBUG_TRACE_D(_EXPR_, _MODULE_, "Recibido topic local %s con mensaje de tamaño '%d'", topic, msg_len);
}

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
            DEBUG_TRACE_I(_EXPR_, _MODULE_, "MQTT_EVENT_CONNECTED");
            isConnected = true;
            ev = MqttConnEvt;
            break;

        case MQTT_EVENT_DISCONNECTED:
            DEBUG_TRACE_I(_EXPR_, _MODULE_, "MQTT_EVENT_DISCONNECTED");
            isConnected = false;
            ev = MqttDiscEvt;
            break;

        case MQTT_EVENT_SUBSCRIBED:
            DEBUG_TRACE_I(_EXPR_, _MODULE_, "MQTT_EVENT_SUBSCRIBED, msgId=%d", event->msg_id);
            topicsSubscribed[event->msg_id]=true;
            ev = MqttSubscrEvt;
            //ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            //msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
            //ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            DEBUG_TRACE_I(_EXPR_, _MODULE_, "MQTT_EVENT_UNSUBSCRIBED");
            //ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            DEBUG_TRACE_I(_EXPR_, _MODULE_, "MQTT_EVENT_PUBLISHED");
            //ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
        {
            DEBUG_TRACE_I(_EXPR_, _MODULE_, "MQTT_EVENT_DATA");
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
            ev = MqttErrorEvt;
            break;
        default:
            DEBUG_TRACE_I(_EXPR_, _MODULE_, "Other event");
            //ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }

    op->sig = ev;
    // apunta a los datos
    op->msg = mdata;

    // postea en la cola de la m�quina de estados
    if(putMessage(op) != osOK){
        DEBUG_TRACE_I(_EXPR_, _MODULE_, "MQTT_EVENT_DATA5");
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

State::StateResult MQTTClient::Init_EventHandler(State::StateEvent* se)
{
    State::Msg* st_msg = (State::Msg*)se->oe->value.p;
    switch((int)se->evt)
    {
        case State::EV_ENTRY:
        {
        	// Marca el estado como conectando
            statusFlag = 0;

        	// realiza la suscripción local ej: "get.set/+/mqtt"
            char* subTopicLocal = (char*)malloc(MQ::MQClient::getMaxTopicLen());
            MBED_ASSERT(subTopicLocal);
            sprintf(subTopicLocal, "set/+/%s", _sub_topic_base);
            if(MQ::MQClient::subscribe(subTopicLocal, &_subscriptionCb) == MQ::SUCCESS){
                DEBUG_TRACE_D(_EXPR_, _MODULE_, "Sucripción LOCAL hecha a %s", subTopicLocal);
            }
            else{
                DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_SUBSC en la suscripción LOCAL a %s", subTopicLocal);
            }

        	sprintf(subTopicLocal, "get/+/%s", _sub_topic_base);
        	if(MQ::MQClient::subscribe(subTopicLocal, &_subscriptionCb) == MQ::SUCCESS){
        		DEBUG_TRACE_D(_EXPR_, _MODULE_, "Sucripción LOCAL hecha a %s", subTopicLocal);
        	}
            else{
        		DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_SUBSC en la suscripción LOCAL a %s", subTopicLocal);
            }

            sprintf(subTopicLocal, "stat/+/+/+/+");
        	if(MQ::MQClient::subscribe(subTopicLocal, &subscriptionToServerCb) == MQ::SUCCESS){
        		DEBUG_TRACE_D(_EXPR_, _MODULE_, "Sucripción LOCAL hecha a %s", subTopicLocal);
        	}
            else{
        		DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_SUBSC en la suscripción LOCAL a %s", subTopicLocal);
            }

        	free(subTopicLocal);

            //carga la configuración y ejecuta el cliente mqtt
            clientHandle = esp_mqtt_client_init(&mqttCfg);
            esp_mqtt_client_start(clientHandle);
            
        	// publica estado inicial
        	notifyConnStatUpdate();

        	return State::HANDLED;
        }

        case MqttConnEvt:
        {
            statusFlag = 1;
        	DEBUG_TRACE_I(_EXPR_, _MODULE_, "Iniciando subscripción a topics del servidor");
            int msgId;
            for(int i=0; i<MaxSubscribedTopics; i++)
            {
                msgId = esp_mqtt_client_subscribe(clientHandle, subscTopic[i], 1);
                DEBUG_TRACE_I(_EXPR_, _MODULE_, "Iniciando subscripción remota a topic: %s, msg_id=%d", subscTopic[i], msgId);
                topicsSubscribed[msgId]=false;
            }

            notifyConnStatUpdate();

            return State::HANDLED;
        }

        case MqttSubscrEvt:
        {
            DEBUG_TRACE_I(_EXPR_, _MODULE_, "Topic subscrito");
            bool isTopicsSubscribed = true;
            for (auto& topic: topicsSubscribed) {
                if(!topic.second)
                    isTopicsSubscribed = false;
            }

            if(isTopicsSubscribed)
            {
                topicsSubscribed.clear();
                //comunicar a mqlib que el modulo está disponible
                statusFlag = 2;
                notifyConnStatUpdate();
                DEBUG_TRACE_I(_EXPR_, _MODULE_, "Todos los topics subscritos");
            }
            else
                DEBUG_TRACE_I(_EXPR_, _MODULE_, "No Todos los topics subscritos");

            return State::HANDLED;
        }

        case MqttDataEvt:
        {
            DEBUG_TRACE_I(_EXPR_, _MODULE_, "Datos recibidos");
            MqttEvtMsg_t* mdata = ((MqttEvtMsg_t*)st_msg->msg);
            MBED_ASSERT(mdata);
            
            MqttTopicData_t* topicData =  (MqttTopicData_t*)mdata->data;
            MBED_ASSERT(topicData);
            
            DEBUG_TRACE_I(_EXPR_, _MODULE_, "EV_DATA recibido topic '%s' con %d bytes, mensaje= '%.*s'", topicData->topic, topicData->data_len, topicData->data_len, topicData->data);

            // procesa el topic recibido, para redireccionarlo localmente
			char* localTopic = (char*)malloc(MQ::MQClient::getMaxTopicLen());
			MBED_ASSERT(localTopic);
			parseMqttTopic(localTopic, topicData->topic);
			// lo redirecciona a MQLib si el topic es correcto
			if(strlen(localTopic) > 0){
				DEBUG_TRACE_I(_EXPR_, _MODULE_, "Reenviando mensaje a topic local '%s'", localTopic);
				int err;
				void* blobData;
				int blobSize;
                
                char* relativeTopic = (char*)malloc(MQ::MQClient::getMaxTopicLen());
                MBED_ASSERT(relativeTopic);
                if(getRelativeTopic(relativeTopic, localTopic))
                {
                    if((blobData = Blob::DecodeJson(relativeTopic, topicData->data, &blobSize, ActiveModule::_defdbg)) != NULL)
                    {
                        if((err = publish(localTopic, blobData, blobSize, &_publicationCb)) != MQ::SUCCESS)
                            DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_MQLIB_PUB al publicar en topic local '%s' con resultado '%d'", localTopic, err);
                        free(blobData);
                    }
                    else{
                        DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_DECODE_JSON al decodificar el objeto");
                    }
                }
                else
                    DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_MQTT al obtener topic relativo a enviar a MQLib");
                free(relativeTopic);
			}
			free(localTopic);

            free(topicData->data);
        	free(topicData);

            return State::HANDLED;
        }

        case MqttPublishToServer:
        {
            DEBUG_TRACE_I(_EXPR_, _MODULE_, "Solicitud de publicar a servidor");
            MqttMsg_t* topicData =  (MqttMsg_t*)st_msg->msg;
            MBED_ASSERT(topicData);

            if(isConnected){
        		char* pubTopic = (char*)malloc(Blob::MaxLengthOfMqttStrings);
        		MBED_ASSERT(pubTopic);
        		parseLocalTopic(pubTopic, topicData->topic);
        		if(strlen(pubTopic) > 0){
					DEBUG_TRACE_I(_EXPR_, _MODULE_, "Publicando en el topic %s", pubTopic);
					char* jsonMsg;
					if((jsonMsg = Blob::ParseJson(topicData->topic, topicData->data, topicData->data_len, ActiveModule::_defdbg)) != NULL)
                    {
                        int msg_id = esp_mqtt_client_publish(clientHandle, pubTopic, jsonMsg, strlen(jsonMsg), 1, 0);
						free(jsonMsg);
					}
        		}
        		else{
        			DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_PARSE. Al convertir el topic '%s' en topic mqtt", topicData->topic);
        		}
        		free(pubTopic);
			}
			else{
				DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_FWD. No se puede enviar la publicaci�n, no hay conexi�n.");
			}

            DEBUG_TRACE_I(_EXPR_, _MODULE_, "lalala1");
        	free(topicData->data);
            DEBUG_TRACE_I(_EXPR_, _MODULE_, "lalala2");
            free(topicData->topic);
            //free(topicData);
            DEBUG_TRACE_I(_EXPR_, _MODULE_, "lalala3");
            
            

        	return State::HANDLED;
        }

        default:
        {
        	return State::IGNORED;
        }
    }
}

void MQTTClient::notifyConnStatUpdate()
{
	DEBUG_TRACE_I(_EXPR_, _MODULE_, "Notificando cambio de estado flags=%d", statusFlag);
    char* pub_topic = (char*)malloc(MQ::MQClient::getMaxTopicLen());
	MBED_ASSERT(pub_topic);
	sprintf(pub_topic, "stat/conn/%s", _pub_topic_base);
	MQ::MQClient::publish(pub_topic, &statusFlag, sizeof(Blob::MqttStatusFlags), &_publicationCb);
	free(pub_topic);
}

void MQTTClient::parseMqttTopic(char* localTopic, const char* mqttTopic)
{
	// por defecto marco resultado como incorrecto
	localTopic[0] = 0;

    if(MQ::MQClient::isTokenRoot(mqttTopic, rootNetworkTopic))
        strcpy(localTopic, &mqttTopic[strlen(rootNetworkTopic)+1]);

	DEBUG_TRACE_I(_EXPR_, _MODULE_, "Conversión de topic mqtt '%s' a topic local '%s'", mqttTopic, localTopic);
}

void MQTTClient::parseLocalTopic(char* mqtt_topic, const char* local_topic){
	// por defecto dejo el topic como inv�lido strlen(mqtt_topic) = 0
	mqtt_topic[0] = 0;

	// realizo el parse de topics dirigidos a otros nodos (acciones set/get)
	if (MQ::MQClient::isTokenRoot(local_topic, "stat/")){
		sprintf(mqtt_topic, "%s/%s", rootNetworkTopic, local_topic);
	}
	DEBUG_TRACE_I(_EXPR_, _MODULE_, "Conversi�n de topic local '%s' a topic mqtt '%s'", local_topic, mqtt_topic);
}

bool MQTTClient::getRelativeTopic(char* relativeTopic, const char* localTopic)
{
    //Quitamos los parametros de grup y device (el segundo y el tercero)
    char* topic = (char*)malloc(strlen(localTopic)+1);
    strcpy(topic, localTopic);
    char *command = strtok(topic, "/");
    bool isCorrect = true;

    sprintf(relativeTopic, "%s/", command);

    for(int i=0; i<2; i++)
    {
        if(command != NULL)
            command = strtok(NULL, "/");
        else
            isCorrect = false;
    }

    if(command != NULL)
        command = strtok(NULL, "");
    else
        isCorrect = false;

    if(command != NULL)
        strcat(relativeTopic, command);
    else
        isCorrect = false;
    DEBUG_TRACE_I(_EXPR_, _MODULE_, "Relative topic: %s", relativeTopic);

    free(topic);

    return isCorrect;
}

void MQTTClient::subscrToServerCb(const char* topic, void* msg, uint16_t msg_len)
{
    DEBUG_TRACE_I(_EXPR_, _MODULE_, "Redireccionando topic %s", topic);
	// crea el mensaje para publicar en la m�quina de estados
	State::Msg* op = (State::Msg*)malloc(sizeof(State::Msg));
	MBED_ASSERT(op);
    
    MqttMsg_t * mq_msg = (MqttMsg_t *)malloc(sizeof(MqttMsg_t));
	MBED_ASSERT(mq_msg);
	mq_msg->topic = (char*)malloc(strlen(topic)+1);
	MBED_ASSERT(mq_msg->topic);
	strcpy(mq_msg->topic, topic);
	mq_msg->data = (char*)malloc(msg_len);
	MBED_ASSERT(mq_msg->data);
	memcpy(mq_msg->data, msg, msg_len);
	mq_msg->topic_len = strlen(topic)+1;
	mq_msg->data_len = msg_len;
    
	op->sig = MqttPublishToServer;
	op->msg = mq_msg;

    // postea en la cola de la m�quina de estados
    if(putMessage(op) != osOK)
    {
        free(mq_msg->topic);
    	free(mq_msg->data);
    	if(op->msg)
    		free(op->msg);
    	free(op);
    }
}


int32_t MQTTClient::publish(const char* topic, void *data, uint32_t dataSize,
    Callback<void(const char*, int32_t)> *publisher)
{
    return myPublisher(topic, data, dataSize, publisher);
}

int32_t MQTTClient::meshPublish (const char* topic, void *data, uint32_t dataSize,
    Callback<void(const char*, int32_t)> *publisher)
{
    DEBUG_TRACE_I(_EXPR_, _MODULE_, "Creando paquete de datos para enviar");
    MeshTopicData_t* meshData = (MeshTopicData_t*)malloc(sizeof(MeshTopicData_t));
    MBED_ASSERT(meshData);
    meshData->data = data;
    MBED_ASSERT(strlen(topic) <= Blob::MaxLengthOfMqttStrings);
    strncpy(meshData->topic, topic, strlen(topic));
    meshData->topic[strlen(topic)] = 0;

    DEBUG_TRACE_I(_EXPR_, _MODULE_, "Creado paquete de datos");

    int ret = mwifi_write(NULL, &data_type, meshData, sizeof(meshData), true);

    DEBUG_TRACE_I(_EXPR_, _MODULE_, "Publicado paquete en la red mesh");

    free(meshData);
    
    DEBUG_TRACE_I(_EXPR_, _MODULE_, "My local publish");
    return ret;
}

osEvent MQTTClient::getOsEvent()
{
    return queueSM.get();
}


bool MQTTClient::checkIntegrity()
{
    bool success = true;

	// verifico cliente mqtt
	#warning TODO habilitar verificaciones...

	// chequeo intergridad
	if(success){
		return true;
	}
	return false;
}

void MQTTClient::setDefaultConfig()
{

}

void MQTTClient::restoreConfig()
{

}

void MQTTClient::saveConfig()
{

}