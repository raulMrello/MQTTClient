#include "MQTTClient.h"

static const char* _MODULE_ = "[MqttCli].......";
#define _EXPR_	(ActiveModule::_defdbg)
#define MQTT_LOCAL_PUBLISHER


State::StateResult MQTTClient::Init_EventHandler(State::StateEvent* se)
{
    State::Msg* st_msg = (State::Msg*)se->oe->value.p;
    switch((int)se->evt)
    {
        case State::EV_ENTRY:
        {
        	DEBUG_TRACE_I(_EXPR_, _MODULE_, "Iniciando recuperaci�n de datos...");

        	// recupera los datos de memoria NV
        	restoreConfig();

        	// Marca el estado como conectando
            connStatus = Blob::Subscribing;

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

            sprintf(subTopicLocal, "stat/+/+/+/+");
        	if(MQ::MQClient::subscribe(subTopicLocal, &subscriptionToServerCb) == MQ::SUCCESS){
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

            _ready = true;

        	return State::HANDLED;
        }

        case MqttConnEvt:
        {
            connStatus = Blob::RequestedDev;
        	DEBUG_TRACE_D(_EXPR_, _MODULE_, "Iniciando subscripción a topics del servidor");
            int msgId;
            for(int i=0; i<MaxSubscribedTopics; i++)
            {
                msgId = esp_mqtt_client_subscribe(clientHandle, subscTopic[i], 1);
                DEBUG_TRACE_D(_EXPR_, _MODULE_, "Iniciando subscripción remota a topic: %s, msg_id=%d", subscTopic[i], msgId);
                topicsSubscribed[msgId]=false;
            }

            notifyConnStatUpdate();

            return State::HANDLED;
        }

        case MqttSubscrEvt:
        {
            bool isTopicsSubscribed = true;
            for (auto& topic: topicsSubscribed) {
                if(!topic.second)
                    isTopicsSubscribed = false;
            }

            if(isTopicsSubscribed)
            {
                topicsSubscribed.clear();
                //comunicar a mqlib que el modulo está disponible
                connStatus = Blob::SubscribedDev;
                notifyConnStatUpdate();
            }

            return State::HANDLED;
        }

        case MqttDataEvt:
        {
            MqttEvtMsg_t* mdata = ((MqttEvtMsg_t*)st_msg->msg);
            MBED_ASSERT(mdata);
            
            MqttTopicData_t* topicData =  (MqttTopicData_t*)mdata->data;
            MBED_ASSERT(topicData);
            
            DEBUG_TRACE_I(_EXPR_, _MODULE_, "EV_DATA recibido topic '%s' con %d bytes, mensaje= '%.*s'", topicData->topic, topicData->data_len, topicData->data_len, topicData->data);

            // procesa el topic recibido, para redireccionarlo localmente
			char* localTopic = (char*)Heap::memAlloc(MQ::MQClient::getMaxTopicLen());
			MBED_ASSERT(localTopic);
			parseMqttTopic(localTopic, topicData->topic);
			// lo redirecciona a MQLib si el topic es correcto
			if(strlen(localTopic) > 0){
				DEBUG_TRACE_D(_EXPR_, _MODULE_, "Reenviando mensaje a topic local '%s'", localTopic);
				
                int err;
				void* blobData;
				int blobSize;
                
                char* relativeTopic = (char*)Heap::memAlloc(MQ::MQClient::getMaxTopicLen());
                MBED_ASSERT(relativeTopic);
                bool isOwnMsg = false;
                
                if(getRelativeTopic(relativeTopic, localTopic, &isOwnMsg))
                {
                    #if defined(BINARY_MESSAGES)
                        blobData = Blob::DecodeJson(relativeTopic, topicData->data, &blobSize, ActiveModule::_defdbg);
                    #else
                        blobData = (char*)Heap::memAlloc(topicData->data_len);
                        strcpy(blobData, topicData->data);
                        blobSize = topicData->data_len;
                    #endif
                    if(blobData != NULL)
                    {
                        // Si el mensaje va dirigido unicamente a este nodo, se envía a MQlib sin
                        // grupo y sin ID para ser procesado a nivel local. En caso de ir dirigido
                        // a más nodos, se enviará tal cual para ser procesado por NetworkManager
                        if(isOwnMsg)
                        {
                            if((err = publish(relativeTopic, blobData, blobSize, &_publicationCb)) != MQ::SUCCESS)
                                DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_MQLIB_PUB al publicar en topic local '%s' con resultado '%d'", localTopic, err);
                        }    
                        else
                        {
                            if((err = publish(localTopic, blobData, blobSize, &_publicationCb)) != MQ::SUCCESS)
                                DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_MQLIB_PUB al publicar en topic local '%s' con resultado '%d'", localTopic, err);
                        }
                    }
                    else{
                        DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_DECODE_JSON al decodificar el objeto");
                    }
                    Heap::memFree(blobData);
                }
                else
                    DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_MQTT al obtener topic relativo a enviar a MQLib");
                Heap::memFree(relativeTopic);
			}
			Heap::memFree(localTopic);

            Heap::memFree(topicData->data);
        	Heap::memFree(topicData);

            return State::HANDLED;
        }

        case MqttPublishToServer:
        {
            DEBUG_TRACE_I(_EXPR_, _MODULE_, "Solicitud de publicar a servidor");
            Blob::BaseMsg_t* topicData =  (Blob::BaseMsg_t*)st_msg->msg;
            MBED_ASSERT(topicData);

            if(isConnected)
            {
                if(checkServerBridge(topicData->topic))
                {
                    char* pubTopic = (char*)Heap::memAlloc(Blob::MaxLengthOfMqttStrings);
                    MBED_ASSERT(pubTopic);
                    parseLocalTopic(pubTopic, topicData->topic);
                    if(strlen(pubTopic) > 0)
                    {
                        #if defined(BINARY_MESSAGES)
                        char* relativeTopic = (char*)Heap::memAlloc(MQ::MQClient::getMaxTopicLen());
                        MBED_ASSERT(relativeTopic);
                        if(getRelativeTopic(relativeTopic, topicData->topic))
                        {
                            char* jsonMsg;
                            if((jsonMsg = Blob::ParseJson(relativeTopic, topicData->data, topicData->data_len, ActiveModule::_defdbg)) != NULL)
                            {
                                int msg_id = esp_mqtt_client_publish(clientHandle, pubTopic, jsonMsg, strlen(jsonMsg), 1, 0);
                                DEBUG_TRACE_I(_EXPR_, _MODULE_, "Publicando en servidor mensaje id:%d con contenido: %s", msg_id, jsonMsg);
                            }
                            Heap::memFree(jsonMsg);
                        }
                        else
                            DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_MQTT al obtener topic relativo a enviar a MQLib");
                        Heap::memFree(relativeTopic);
                        #else
                        if(_json_supported){
                            char* jsonMsg = cJSON_PrintUnformatted(*(cJSON**)topicData->data);
                            DEBUG_TRACE_I(_EXPR_, _MODULE_, "Publicando en servidor mensaje con contenido: %s", jsonMsg);
                            int msg_id = esp_mqtt_client_publish(clientHandle, pubTopic, jsonMsg, strlen(jsonMsg)+1, 1, 0);
                            DEBUG_TRACE_I(_EXPR_, _MODULE_, "Publicando en servidor mensaje id:%d con contenido: %s", msg_id, jsonMsg);
                            Heap::memFree(jsonMsg);
                        }
                        else{
                            int msg_id = esp_mqtt_client_publish(clientHandle, pubTopic, topicData->data, topicData->data_len, 1, 0);
                            DEBUG_TRACE_I(_EXPR_, _MODULE_, "Publicando en servidor mensaje id:%d", msg_id);
                        }
                        #endif
                    }
                    else{
                        DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_PARSE. Al convertir el topic '%s' en topic mqtt", topicData->topic);
                    }
                    Heap::memFree(pubTopic);
                }
                else
                {
                    DEBUG_TRACE_D(_EXPR_, _MODULE_, "El topic %s no está añadido en la lista de bridges de MQTTClient hacia el servidor", topicData->topic);
                }
			}
			else{
				DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_FWD. No se puede enviar la publicaci�n, no hay conexi�n.");
			}

            Heap::memFree(topicData->data);
            Heap::memFree(topicData->topic);


        	return State::HANDLED;
        }

        case MqttErrorEvt:
            return State::HANDLED;

		// Procesa datos recibidos de la publicaci�n en cmd/$BASE/cfg/set
		case RecvCfgSet:{
			Blob::SetRequest_t<Blob::MQTTCfgData_t>* req = (Blob::SetRequest_t<Blob::MQTTCfgData_t>*)st_msg->msg;
			DEBUG_TRACE_I(_EXPR_, _MODULE_, "Recibida nueva configuraci�n keys=%x", req->keys);
			// si no hay errores, actualiza la configuraci�n
			if(req->_error.code == Blob::ErrOK){
				_updateConfig(req->data, req->keys, req->_error);
			}
			// si hay errores en el mensaje o en la actualizaci�n, devuelve resultado sin hacer nada
			if(req->_error.code != Blob::ErrOK){
				DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_UPD al actualizar code=%d", req->_error.code);
				char* pub_topic = (char*)Heap::memAlloc(MQ::MQClient::getMaxTopicLen());
				MBED_ASSERT(pub_topic);
				sprintf(pub_topic, "stat/cfg/%s", _pub_topic_base);

				Blob::Response_t<Blob::MQTTCfgData_t>* resp = new Blob::Response_t<Blob::MQTTCfgData_t>(req->idTrans, req->_error, mqttLocalCfg);

				if(_json_supported){
					cJSON* jresp = JsonParser::getJsonFromResponse(*resp);
					if(jresp){
						char* jmsg = cJSON_PrintUnformatted(jresp);
						cJSON_Delete(jresp);
						MQ::MQClient::publish(pub_topic, jmsg, strlen(jmsg)+1, &_publicationCb);
						Heap::memFree(jmsg);
						delete(resp);
						Heap::memFree(pub_topic);
						return State::HANDLED;
					}
				}
                else{
                    MQ::MQClient::publish(pub_topic, resp, sizeof(Blob::Response_t<Blob::MQTTCfgData_t>), &_publicationCb);
                    delete(resp);
                    Heap::memFree(pub_topic);
                    return State::HANDLED;
                }
			}

			// almacena en el sistema de ficheros
			saveConfig();
			DEBUG_TRACE_I(_EXPR_, _MODULE_, "Config actualizada");

			// si est� habilitada la notificaci�n de actualizaci�n, lo notifica
			if((mqttLocalCfg.updFlagMask & Blob::EnableMqttCfgUpdNotif) != 0){
				char* pub_topic = (char*)Heap::memAlloc(MQ::MQClient::getMaxTopicLen());
				MBED_ASSERT(pub_topic);
				sprintf(pub_topic, "stat/cfg/%s", _pub_topic_base);

				Blob::Response_t<Blob::MQTTCfgData_t>* resp = new Blob::Response_t<Blob::MQTTCfgData_t>(req->idTrans, req->_error, mqttLocalCfg);

				if(_json_supported){
					cJSON* jresp = JsonParser::getJsonFromResponse(*resp);
					if(jresp){
						char* jmsg = cJSON_PrintUnformatted(jresp);
						cJSON_Delete(jresp);
						MQ::MQClient::publish(pub_topic, jmsg, strlen(jmsg)+1, &_publicationCb);
						Heap::memFree(jmsg);
						delete(resp);
						Heap::memFree(pub_topic);
						return State::HANDLED;
					}
				}
                else{
                    MQ::MQClient::publish(pub_topic, resp, sizeof(Blob::Response_t<Blob::MQTTCfgData_t>), &_publicationCb);
                    delete(resp);
                    Heap::memFree(pub_topic);
                }
			}

			return State::HANDLED;
		}

		// Procesa datos recibidos de la publicaci�n en cmd/$BASE/cfg/get
		case RecvCfgGet:{
			Blob::GetRequest_t* req = (Blob::GetRequest_t*)st_msg->msg;
			// prepara el topic al que responder
			char* pub_topic = (char*)Heap::memAlloc(MQ::MQClient::getMaxTopicLen());
			MBED_ASSERT(pub_topic);
			sprintf(pub_topic, "stat/cfg/%s", _pub_topic_base);

			// responde con los datos solicitados y con los errores (si hubiera) de la decodificaci�n de la solicitud
			Blob::Response_t<Blob::MQTTCfgData_t>* resp = new Blob::Response_t<Blob::MQTTCfgData_t>(req->idTrans, req->_error, mqttLocalCfg);

			if(_json_supported){
				cJSON* jresp = JsonParser::getJsonFromResponse(*resp);
				if(jresp){
					char* jmsg = cJSON_PrintUnformatted(jresp);
					cJSON_Delete(jresp);
					MQ::MQClient::publish(pub_topic, jmsg, strlen(jmsg)+1, &_publicationCb);
					Heap::memFree(jmsg);
					delete(resp);
					Heap::memFree(pub_topic);
					return State::HANDLED;
				}
			}
            else{
                MQ::MQClient::publish(pub_topic, resp, sizeof(Blob::Response_t<Blob::MQTTCfgData_t>), &_publicationCb);
                delete(resp);

                // libera la memoria asignada al topic de publicaci�n
                Heap::memFree(pub_topic);

                DEBUG_TRACE_I(_EXPR_, _MODULE_, "Enviada respuesta con la configuraci�n solicitada");
                return State::HANDLED;
            }
		}

        default:
        {
        	return State::IGNORED;
        }
    }
}

