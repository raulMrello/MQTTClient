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
  

namespace Blob 
{

	/** Tama�o m�ximo de las cadenas de texto relacionadas con par�metros del cliente mqtt */
	static const uint8_t MaxLengthOfMqttStrings = 64;
	static const uint8_t MaxLengthOfLoginStrings = 16;

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
		MqttKeyCfgUsername	= (1 << 4),
		MqttKeyCfgPasswd	= (1 << 5),
		MqttKeyCfgVerbosity	= (1 << 6),
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
		char username[MaxLengthOfLoginStrings];
		char passwd[MaxLengthOfLoginStrings];
		esp_log_level_t verbosity;	//!< Nivel de verbosity para las trazas de depuraci�n
	};
}

namespace JSON 
{
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
	cJSON* getJsonFromMQTTCliStat(const Blob::MqttStatusFlags& stat);

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
	uint32_t getMQTTCliStatFromJson(Blob::MqttStatusFlags &obj, cJSON* json);
}

#endif
