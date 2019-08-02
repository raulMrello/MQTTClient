/*
 * MQTTClient_BLOB.h
 *
 *  Created on: Ene 2018
 *      Author: raulMrello
 *
 *	MQTTClient_BLOB es el componente del m�dulo MQTTClient en el que se definen los objetos y tipos relativos a
 *	los objetos BLOB de este m�dulo.
 *	Todos los tipos definidos en este componente est�n asociados al namespace "Blob", de forma que puedan ser
 *	accesibles mediante el uso de: "Blob::"  e importando este archivo de cabecera.
 */
 
#ifndef __MQTTClient_BLOB__H
#define __MQTTClient_BLOB__H

#include "Blob.h"
#include "mbed.h"
#include <vector>
#include "common_objects.h"

/** Versiones soportadas */
#define VERS_MQTT_YTL			0


/** Selecci�n de la versi�n utilizada 	*/
/** DEFINIR SEG�N APLICACI�N 			*/
#define VERS_MQTT_SELECTED		VERS_MQTT_YTL /*others...*/


/** Macro de generaci�n de UIDs*/
#define UID_MQTT_MANAGER		(uint32_t)(0x00000007 | ((uint32_t)VERS_MQTT_SELECTED << 20))


namespace Blob 
{

	/** Tama�o m�ximo de las cadenas de texto relacionadas con par�metros del cliente mqtt */
	static const uint8_t MaxLengthOfMqttStrings = 64;
	static const uint8_t MaxLengthOfLoginStrings = 16;
	static const uint8_t MaxLengthOfUserLength = 32;
	static const uint8_t MaxLengthOfPassLength = 64;

	/** Estados del cliente MQTT */
	enum MqttStatusFlags
	{
		Subscribing     = (1 << 0), //!< Conectando
		RequestedDev	= (1 << 1),	//!< solicitada suscripci�n al topic X
		SubscribedDev	= (1 << 5),//!< Suscrito al topic X
	};

	/** Flags para la configuraci�n de notificaciones cuando su configuraci�n se ha modificado. */
	enum MqttUpdFlags{
		EnableMqttCfgUpdNotif 	= (1 << 0),  	/// Flag activado para notificar cambios en la configuraci�n en bloque del objeto
	};


	/** Flags para identificar cada key-value de los objetos JSON que se han modificado en un SET remoto */
	enum MqttKeyNames{
		MqttKeyNone 		= 0,
		MqttKeyCfgUpd		= (1 << 0),
		MqttKeyCfgGrpMsk	= (1 << 1),
		MqttKeyCfgKeepAlive	= (1 << 2),
		MqttKeyCfgQos		= (1 << 3),
		MqttKeyCfgUrl		= (1 << 4),
		MqttKeyCfgPort		= (1 << 5),
		MqttKeyCfgUsername	= (1 << 6),
		MqttKeyCfgPasswd	= (1 << 7),
		MqttKeyCfgVerbosity	= (1 << 8),
		//
		MqttKeyCfgAll     = 0x7f,
	};

	/** Estructura de datos de configuraci�n del cliente MQTT */
	struct MQTTCfgData_t
	{
		MqttUpdFlags updFlagMask;
		uint16_t keepAlive;
		uint32_t groupMask;
		int qos;
		
		char mqttUrl[MaxLengthOfMqttStrings];			//!< URL del servidor MQTT
		int mqttPort;							//!< Puerto de conexi�n con el servidor MQTT
		char mqttUser[MaxLengthOfUserLength];		//!< Usuario del cliente MQTT
		char mqttPass[MaxLengthOfPassLength];		//!< Password del cliente MQTT

		char username[MaxLengthOfLoginStrings];
		char passwd[MaxLengthOfLoginStrings];
		esp_log_level_t verbosity;	//!< Nivel de verbosity para las trazas de depuraci�n
		 uint32_t _keys;
	};
	struct MQTTStatData_t
	{
		Blob::MqttStatusFlags connStatus;
		bool isConnected;
	};

	struct MQTTBootData_t{
		uint32_t uid;
		MQTTCfgData_t cfg;
		MQTTStatData_t stat;
	};
}

typedef Blob::MQTTBootData_t 	mqtt_manager;
typedef Blob::MQTTCfgData_t 	mqtt_manager_cfg;
typedef Blob::MQTTStatData_t 	mqtt_manager_stat;

namespace JSON {

/**
 * Codifica el objeto en un JSON
 * @param obj Objeto
 * @return JSON resultante o NULL en caso de error
 */
cJSON* getJsonFromMQTTCli(const Blob::MQTTBootData_t& obj, ObjDataSelection type);

/**
 * Codifica la configuraci�n actual en un objeto JSON
 * @param cfg Configuraci�n
 * @return Objeto JSON o NULL en caso de error
 */
cJSON* getJsonFromMQTTCliCfg(const Blob::MQTTCfgData_t& cfg);

/**
 * Codifica el estado actual en un objeto JSON
 * @param stat Estado
 * @return Objeto JSON o NULL en caso de error
 */
cJSON* getJsonFromMQTTCliStat(const Blob::MQTTStatData_t& stat);

cJSON* getJsonFromMQTTCliConnStat(const Blob::MqttStatusFlags& stat);


/**
 * Codifica el objeto en un JSON dependiendo del tipo de objeto
 * @param obj Objeto
 * @return JSON resultante o NULL en caso de error
 */
template <typename T>
cJSON* getJsonFromMQTTCli(const T& obj, ObjDataSelection type){
	if (std::is_same<T, Blob::MQTTBootData_t>::value){
		return getJsonFromMQTTCli((const Blob::MQTTBootData_t&)obj, type);
	}
	if (std::is_same<T, Blob::MQTTCfgData_t>::value && type != ObjSelectState){
		return getJsonFromMQTTCliCfg((const Blob::MQTTCfgData_t&)obj);
	}
	if (std::is_same<T, Blob::MQTTStatData_t>::value && type != ObjSelectCfg){
		return getJsonFromMQTTCliStat((const Blob::MQTTStatData_t&)obj);
	}
	if (std::is_same<T, Blob::MqttStatusFlags>::value){
		return getJsonFromMQTTCliConnStat((const Blob::MqttStatusFlags&)obj);
	}
	return NULL;
}


/**
 * Decodifica el mensaje JSON en un objeto
 * @param obj Recibe el objeto decodificado
 * @param json Objeto JSON a decodificar
 * @return keys Par�metros decodificados o 0 en caso de error
 */
uint32_t getMQTTCliFromJson(Blob::MQTTBootData_t &obj, cJSON* json);


/**
 * Decodifica el mensaje JSON en un objeto de configuraci�n
 * @param obj Recibe el objeto decodificado
 * @param json Objeto JSON a decodificar
 * @return keys Par�metros decodificados o 0 en caso de error
 */
uint32_t getMQTTCliCfgFromJson(Blob::MQTTCfgData_t &obj, cJSON* json);

/**
 * Decodifica el mensaje JSON en un objeto de estado
 * @param obj Recibe el objeto decodificado
 * @param json Objeto JSON a decodificar
 * @return keys Par�metros decodificados o 0 en caso de error
 */
uint32_t getMQTTCliStatFromJson(Blob::MQTTStatData_t &obj, cJSON* json);

uint32_t getMQTTCliConnStatFromJson(Blob::MqttStatusFlags &obj, cJSON* json);

template <typename T>
uint32_t getMQTTCliObjFromJson(T& obj, cJSON* json_obj){
	if (std::is_same<T, Blob::MQTTBootData_t>::value){
		return JSON::getMQTTCliFromJson((Blob::MQTTBootData_t&)obj, json_obj);
	}
	if (std::is_same<T, Blob::MQTTCfgData_t>::value){
		return JSON::getMQTTCliCfgFromJson((Blob::MQTTCfgData_t&)obj, json_obj);
	}
	if (std::is_same<T, Blob::MQTTStatData_t>::value){
		return JSON::getMQTTCliStatFromJson((Blob::MQTTStatData_t&)obj, json_obj);
	}
	if (std::is_same<T, Blob::MqttStatusFlags>::value){
		return JSON::getMQTTCliConnStatFromJson((Blob::MqttStatusFlags&)obj, json_obj);
	}

	return 0;
}


}

#endif
