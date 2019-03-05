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

	//------------------------------------------------------------------------------------
	cJSON* getJsonFromMQTTCliCfg(const Blob::MQTTCfgData_t& cfg){
		cJSON* mqtt = NULL;
		cJSON* value = NULL;

		if((mqtt=cJSON_CreateObject()) == NULL){
			return NULL;
		}
		// key: keepAlive
		cJSON_AddNumberToObject(mqtt, JsonParser::p_keepAlive, cfg.keepAlive);

		// key: groupMask
		cJSON_AddNumberToObject(mqtt, JsonParser::p_groupMask, cfg.groupMask);

		// key: qos
		cJSON_AddNumberToObject(mqtt, JsonParser::p_qos, cfg.qos);

		// key: verbosity
		cJSON_AddNumberToObject(mqtt, JsonParser::p_verbosity, cfg.verbosity);

		// key: username
		if((value=cJSON_CreateString(cfg.username)) == NULL){
			cJSON_Delete(mqtt);
			return NULL;
		}
		cJSON_AddItemToObject(mqtt, JsonParser::p_username, value);
		// key: passwd
		if((value=cJSON_CreateString(cfg.passwd)) == NULL){
			cJSON_Delete(mqtt);
			return NULL;
		}
		cJSON_AddItemToObject(mqtt, JsonParser::p_passwd, value);
		return mqtt;
	}


	//------------------------------------------------------------------------------------
    cJSON* getJsonFromMQTTCliStat(const Blob::MqttStatusFlags& stat){
        cJSON* json = NULL;

        if((json=cJSON_CreateObject()) == NULL){
            return NULL;
        }
        // key: flags
        cJSON_AddNumberToObject(json, JsonParser::p_flags, stat);
        return json;
    }


    //------------------------------------------------------------------------------------
    uint32_t getMQTTCliCfgFromJson(Blob::MQTTCfgData_t &cfg, cJSON* json){
    	cJSON *obj = NULL;
    	uint32_t keys = Blob::MqttKeyNone;

    	if((obj = cJSON_GetObjectItem(json,JsonParser::p_updFlags)) != NULL){
    		cfg.updFlagMask = obj->valueint;
    		keys |= Blob::MqttKeyCfgUpd;
    	}
    	if((obj = cJSON_GetObjectItem(json,JsonParser::p_groupMask)) != NULL){
    		cfg.groupMask = obj->valueint;
    		keys |= Blob::MqttKeyCfgGrpMsk;
    	}
    	if((obj = cJSON_GetObjectItem(json,JsonParser::p_keepAlive)) != NULL){
    		cfg.keepAlive = obj->valueint;
    		keys |= Blob::MqttKeyCfgKeepAlive;
    	}
    	if((obj = cJSON_GetObjectItem(json,JsonParser::p_qos)) != NULL){
    		cfg.qos = obj->valueint;
    		keys |= Blob::MqttKeyCfgQos;
    	}
    	if((obj = cJSON_GetObjectItem(json,JsonParser::p_verbosity)) != NULL){
    		cfg.verbosity = obj->valueint;
    		keys |= Blob::MqttKeyCfgVerbosity;
    	}
    	if((obj = cJSON_GetObjectItem(json, JsonParser::p_username)) != NULL){
			char* str = obj->valuestring;
			if(str && strlen(str) < Blob::MaxLengthOfLoginStrings){
				strncpy(cfg.username, str, Blob::MaxLengthOfLoginStrings);
				keys |= Blob::MqttKeyCfgUsername;
			}
		}
    	if((obj = cJSON_GetObjectItem(json, JsonParser::p_passwd)) != NULL){
			char* str = obj->valuestring;
			if(str && strlen(str) < Blob::MaxLengthOfLoginStrings){
				strncpy(cfg.passwd, str, Blob::MaxLengthOfLoginStrings);
				keys |= Blob::MqttKeyCfgPasswd;
			}
		}

    	return keys;
    }


	//------------------------------------------------------------------------------------
    uint32_t getMQTTCliStatFromJson(Blob::MqttStatusFlags &stat, cJSON* json){
        cJSON *obj = NULL;
        if((obj = cJSON_GetObjectItem(json, JsonParser::p_flags)) == NULL){
            DEBUG_TRACE_E(_EXPR_, _MODULE_, "getMQTTCliStatFromJson: flags no existe");
            return 0;
        }
        stat = obj->valueint;
        return 1;
    }
}
