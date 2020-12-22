#include "MQTTClient.h"

/** Macro para imprimir trazas de depuraci�n, siempre que se haya configurado un objeto
 *	Logger v�lido (ej: _debug)
 */
static const char* _MODULE_ = "[MqttCli].......";
#define _EXPR_	(ActiveModule::_defdbg)
#define MQTT_LOCAL_PUBLISHER



void MQTTClient::subscrToServerCb(const char* topic, void* msg, uint16_t msg_len)
{
    // crea el mensaje para publicar en la m�quina de estados
	State::Msg* op = (State::Msg*)Heap::memAlloc(sizeof(State::Msg));
	MBED_ASSERT(op);
    
    Blob::BaseMsg_t * mq_msg = (Blob::BaseMsg_t *)Heap::memAlloc(sizeof(Blob::BaseMsg_t));
	MBED_ASSERT(mq_msg);
	mq_msg->topic = (char*)Heap::memAlloc(strlen(topic)+1);
	MBED_ASSERT(mq_msg->topic);
	strcpy(mq_msg->topic, topic);
	mq_msg->data = (void*)Heap::memAlloc(msg_len);
	MBED_ASSERT(mq_msg->data);
	memcpy(mq_msg->data, msg, msg_len);
	mq_msg->topic_len = strlen(topic)+1;
	mq_msg->data_len = msg_len;
    
	op->sig = MqttPublishToServer;
	op->msg = mq_msg;

    // postea en la cola de la m�quina de estados
    if(putMessage(op) != osOK)
    {
        Heap::memFree(mq_msg->topic);
    	Heap::memFree(mq_msg->data);
    	if(op->msg)
    		Heap::memFree(op->msg);
    	Heap::memFree(op);
    }
}

void MQTTClient::subscriptionCb(const char* topic, void* msg, uint16_t msg_len)
{
    DEBUG_TRACE_D(_EXPR_, _MODULE_, "Recibido topic local %s con mensaje de tamaa�o '%d'", topic, msg_len);

    // si es un comando para actualizar en bloque toda la configuraci�n...
    if(MQ::MQClient::isTokenRoot(topic, "set/cfg")){
        DEBUG_TRACE_D(_EXPR_, _MODULE_, "Recibido topic %s", topic);

        Blob::SetRequest_t<mqtt_manager>* req = NULL;
        bool json_decoded = false;
		if(_json_supported){
			req = (Blob::SetRequest_t<mqtt_manager>*)Heap::memAlloc(sizeof(Blob::SetRequest_t<mqtt_manager>));
			MBED_ASSERT(req);
			cJSON* msgDup = *(cJSON**)msg;
			if(!(json_decoded = JsonParser::getSetRequestFromJson(*req, msgDup))){
				Heap::memFree(req);
			}
		}

        // Antes de nada, chequea que el tama�o de la zona horaria es correcto, en caso contrario, descarta el topic
        if(!json_decoded && msg_len != sizeof(Blob::SetRequest_t<mqtt_manager>)){
        	DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_MSG. Error en el n� de datos del mensaje, topic [%s]", topic);
			return;
        }

        // crea el mensaje para publicar en la m�quina de estados
        State::Msg* op = (State::Msg*)Heap::memAlloc(sizeof(State::Msg));
        MBED_ASSERT(op);

        // el mensaje es un blob tipo mqtt_manager
        if(!json_decoded){
        	req = (Blob::SetRequest_t<mqtt_manager>*)Heap::memAlloc(sizeof(Blob::SetRequest_t<mqtt_manager>));
        	MBED_ASSERT(req);
        	*req = *((Blob::SetRequest_t<mqtt_manager>*)msg);
        }
        op->sig = RecvCfgSet;
		// apunta a los datos
		op->msg = req;

		// postea en la cola de la m�quina de estados
		if(putMessage(op) != osOK){
			DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_PUT. al procesar el topic[%s]", topic);
			if(op->msg){
				Heap::memFree(op->msg);
			}
			Heap::memFree(op);
		}
        return;
    }

    // si es un comando para solicitar la configuraci�n
    if(MQ::MQClient::isTokenRoot(topic, "get/cfg")){
        DEBUG_TRACE_D(_EXPR_, _MODULE_, "Recibido topic %s", topic);

        Blob::GetRequest_t* req = NULL;
        bool json_decoded = false;
        if(_json_supported){
			req = (Blob::GetRequest_t*)Heap::memAlloc(sizeof(Blob::GetRequest_t));
			MBED_ASSERT(req);
			cJSON* msgDup = *(cJSON**)msg;
			if(!(json_decoded = JsonParser::getObjFromJson(*req, msgDup))){
				Heap::memFree(req);
			}
        }

        // Antes de nada, chequea que el tama�o de la zona horaria es correcto, en caso contrario, descarta el topic
        if(!json_decoded && msg_len != sizeof(Blob::GetRequest_t)){
        	DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_MSG. Error en el n� de datos del mensaje, topic [%s]", topic);
			return;
        }

        // crea el mensaje para publicar en la m�quina de estados
        State::Msg* op = (State::Msg*)Heap::memAlloc(sizeof(State::Msg));
        MBED_ASSERT(op);

        // el mensaje es un blob tipo Blob::GetRequest_t
        if(!json_decoded){
        	req = (Blob::GetRequest_t*)Heap::memAlloc(sizeof(Blob::GetRequest_t));
        	MBED_ASSERT(req);
        	*req = *((Blob::GetRequest_t*)msg);
        }
		op->sig = RecvCfgGet;
		// apunta a los datos
		op->msg = req;

		// postea en la cola de la m�quina de estados
		if(putMessage(op) != osOK){
			DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_PUT. al procesar el topic[%s]", topic);
			if(op->msg){
				Heap::memFree(op->msg);
			}
			Heap::memFree(op);
		}
        return;
    }
    // si es un comando para solicitar value
	if(MQ::MQClient::isTokenRoot(topic, "get/value")){
		DEBUG_TRACE_D(_EXPR_, _MODULE_, "Recibido topic %s", topic);

		Blob::GetRequest_t* req = NULL;
		bool json_decoded = false;
		if(_json_supported){
			req = (Blob::GetRequest_t*)Heap::memAlloc(sizeof(Blob::GetRequest_t));
			MBED_ASSERT(req);
			cJSON* msgDup = *(cJSON**)msg;
			if(!(json_decoded = JsonParser::getObjFromJson(*req, msgDup))){
				Heap::memFree(req);
			}
		}

		// Antes de nada, chequea que el tama�o de la zona horaria es correcto, en caso contrario, descarta el topic
		if(!json_decoded && msg_len != sizeof(Blob::GetRequest_t)){
			DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_MSG. Error en el n� de datos del mensaje, topic [%s]", topic);
			return;
		}

		// crea el mensaje para publicar en la m�quina de estados
		State::Msg* op = (State::Msg*)Heap::memAlloc(sizeof(State::Msg));
		MBED_ASSERT(op);

		// el mensaje es un blob tipo Blob::GetRequest_t
		if(!json_decoded){
			req = (Blob::GetRequest_t*)Heap::memAlloc(sizeof(Blob::GetRequest_t));
			MBED_ASSERT(req);
			*req = *((Blob::GetRequest_t*)msg);
		}
		op->sig = RecvValueGet;
		// apunta a los datos
		op->msg = req;

		// postea en la cola de la m�quina de estados
		if(putMessage(op) != osOK){
			DEBUG_TRACE_E(_EXPR_, _MODULE_, "ERR_PUT. al procesar el topic[%s]", topic);
			if(op->msg){
				Heap::memFree(op->msg);
			}
			Heap::memFree(op);
		}
		return;
	}

}




