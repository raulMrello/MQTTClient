/*
 * MQTTClient_Json.cpp
 *
 * Implementaci�n de los codecs JSON-OBJ
 *
 *  Created on: Feb 2019
 *      Author: alvaroSaez
 */

#include "JsonParserBlob.h"

static const char* _MODULE_ = "[MQTTClient_Json]....";
#define _EXPR_	(true)

namespace JSON 
{
    cJSON* getJsonFromMQTTCliStat(const Blob::MqttStatusFlags& stat){
        cJSON* json = NULL;
        cJSON* job = NULL;

        if((json=cJSON_CreateObject()) == NULL){
            return NULL;
        }
        // key: flags
        cJSON_AddNumberToObject(json, JsonParser::p_flags, stat);
        return json;
    }


    uint32_t getMQTTCliStatFromJson(Blob::MqttStatusFlags &stat, cJSON* json){
        cJSON *obj = NULL;
        DEBUG_TRACE_I(_EXPR_, _MODULE_, "Decodificando MQTTCliStatData de un objeto Json");
        if((obj = cJSON_GetObjectItem(json, JsonParser::p_flags)) == NULL){
            DEBUG_TRACE_E(_EXPR_, _MODULE_, "flags no existe");
            return 0;
        }
        stat = obj->valueint;
        return 1;
    }
}