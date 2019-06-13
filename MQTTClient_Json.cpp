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

namespace JSON {

//------------------------------------------------------------------------------------
cJSON* getJsonFromMQTTCli(const Blob::MQTTBootData_t& obj, ObjDataSelection type){
	cJSON* json = NULL;
	cJSON* item = NULL;
	if((json=cJSON_CreateObject()) == NULL){
		return NULL;
	}

	// uid
	cJSON_AddNumberToObject(json, JsonParser::p_uid, obj.uid);

	// cfg
	if(type != ObjSelectState){
		if((item = getJsonFromMQTTCliCfg(obj.cfg)) == NULL){
			cJSON_Delete(json);
			return NULL;
		}
		cJSON_AddItemToObject(json, JsonParser::p_cfg, item);
	}

	// stat
	if(type != ObjSelectCfg){
		if((item = getJsonFromMQTTCliStat(obj.stat)) == NULL){
			cJSON_Delete(json);
			return NULL;
		}
		cJSON_AddItemToObject(json, JsonParser::p_stat, item);
	}

	return json;
}

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
cJSON* getJsonFromMQTTCliStat(const Blob::MQTTStatData_t& stat){
	cJSON* json = NULL;

	if((json=cJSON_CreateObject()) == NULL){
		return NULL;
	}
	// key: flags
	cJSON_AddNumberToObject(json, JsonParser::p_flags, stat.connStatus);
	return json;
}


//------------------------------------------------------------------------------------
uint32_t getMQTTCliFromJson(Blob::MQTTBootData_t &obj, cJSON* json){
	uint32_t keys = 0;
	uint32_t subkey = 0;
	cJSON* value = NULL;
	if(json == NULL){
		return 0;
	}

	// uid
	if((value = cJSON_GetObjectItem(json,JsonParser::p_uid)) != NULL){
		obj.uid = value->valueint;
		keys |= (1 << 0);
	}
	// cfg
	if((value = cJSON_GetObjectItem(json, JsonParser::p_cfg)) != NULL){
		subkey = getMQTTCliCfgFromJson(obj.cfg, value)? (1 << 1) : 0;
		keys |= subkey;
	}
	// stat
	if((value = cJSON_GetObjectItem(json, JsonParser::p_stat)) != NULL){
		subkey = getMQTTCliStatFromJson(obj.stat, value)? (1 << 2) : 0;
		keys |= subkey;
	}

	return keys;
}

//------------------------------------------------------------------------------------
uint32_t getMQTTCliCfgFromJson(Blob::MQTTCfgData_t &cfg, cJSON* json){
	cJSON *obj = NULL;
	uint32_t keys = Blob::MqttKeyNone;
	cfg._keys = 0;

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
	cfg._keys = keys;
	return keys;
}


//------------------------------------------------------------------------------------
uint32_t getMQTTCliStatFromJson(Blob::MQTTStatData_t &stat, cJSON* json){
	cJSON *obj = NULL;
	if((obj = cJSON_GetObjectItem(json, JsonParser::p_flags)) == NULL){
		DEBUG_TRACE_E(_EXPR_, _MODULE_, "getMQTTCliStatFromJson: flags no existe");
		return 0;
	}
	stat.connStatus = obj->valueint;
	return 1;
}


}
