#include "MQTTClient.h"

/** Macro para imprimir trazas de depuraci�n, siempre que se haya configurado un objeto
 *	Logger v�lido (ej: _debug)
 */
static const char* _MODULE_ = "[MqttCli].......";
#define _EXPR_	(ActiveModule::_defdbg)

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
    strcpy(this->rootTopic, rootTopic);
    strcpy(this->networkId, networkId);

    // utiliza el networkId para la suscripción al topic de dispositivo mqtt: cmd/dev/$(networkId)/#
	sprintf(subscTopic[0], "%s/%s/get/#", rootTopic, networkId);
    sprintf(subscTopic[1], "%s/%s/set/#", rootTopic, networkId);

    msgCounter = 0;
    // Carga callbacks estáticas de publicación/suscripción
    _publicationCb = callback(this, &MQTTClient::publicationCb);
    _subscriptionCb = callback(this, &MQTTClient::subscriptionCb);

    clientHandle = NULL;
    mqttCfg = {};

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

void MQTTClient::notifyConnStatUpdate()
{
	DEBUG_TRACE_D(_EXPR_, _MODULE_, "Notificando cambio de estado flags");
	/*DEBUG_TRACE_D(_EXPR_, _MODULE_, "Notificando cambio de estado flags=%d", _conn.flags);
    char* pub_topic = (char*)Heap::memAlloc(MQ::MQClient::getMaxTopicLen());
	MBED_ASSERT(pub_topic);
	sprintf(pub_topic, "stat/conn/%s", _pub_topic_base);
	MQ::MQClient::publish(pub_topic, &_conn.flags, sizeof(Blob::MqttStatusFlags), &_publicationCb);
	Heap::memFree(pub_topic);*/
}

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
    int msgId;
    // your_context_t *context = event->context;
    switch (event->event_id) 
    {
        case MQTT_EVENT_CONNECTED:
            DEBUG_TRACE_I(_EXPR_, _MODULE_, "MQTT_EVENT_CONNECTED");
            ev = MqttConnEvt;
            break;

        case MQTT_EVENT_DISCONNECTED:
            DEBUG_TRACE_I(_EXPR_, _MODULE_, "MQTT_EVENT_DISCONNECTED");
            ev = MqttDiscEvt;
            break;

        case MQTT_EVENT_SUBSCRIBED:
            DEBUG_TRACE_I(_EXPR_, _MODULE_, "MQTT_EVENT_SUBSCRIBED");
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
            DEBUG_TRACE_I(_EXPR_, _MODULE_, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            break;
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
    	if(ev == MqttPublEvt){
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

State::StateResult MQTTClient::Init_EventHandler(State::StateEvent* se)
{
    State::Msg* st_msg = (State::Msg*)se->oe->value.p;
    switch((int)se->evt)
    {
        case State::EV_ENTRY:
        {
        	// restaura la configuración
        	//restoreConfig();

        	// realiza la suscripción local ej: "get.set/+/mqtt"
            char* subTopicLocal = (char*)Heap::memAlloc(MQ::MQClient::getMaxTopicLen());
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

        	Heap::memFree(subTopicLocal);

            //carga la configuración y ejecuta el cliente mqtt
            clientHandle = esp_mqtt_client_init(&mqttCfg);
            esp_mqtt_client_start(clientHandle);
            
        	// publica estado inicial
        	notifyConnStatUpdate();

        	return State::HANDLED;
        }

        case MqttConnEvt:
        {
        	DEBUG_TRACE_I(_EXPR_, _MODULE_, "Iniciando subscripción a topics del servidor");
            int msgId;
            for(int i=0; i<MaxSubscribedTopics; i++)
            {
                msgId = esp_mqtt_client_subscribe(clientHandle, subscTopic[i], 1);
                DEBUG_TRACE_I(_EXPR_, _MODULE_, "Iniciando subscripción remota a topic: %s, msg_id=%d", subscTopic[i], msgId);
            }

            return State::HANDLED;
        }

        default:
        {
        	return State::IGNORED;
        }
    }
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