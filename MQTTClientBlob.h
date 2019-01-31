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
  

namespace Blob {


/** Tama�o m�ximo de las cadenas de texto relacionadas con par�metros del cliente mqtt */
static const uint8_t MaxLengthOfMqttStrings = 64;

/** Estados del cliente MQTT */
enum MqttStatusFlags{
	Subscribing     = (1 << 0), //!< Conectando
	RequestedDev	= (1 << 1),	//!< solicitada suscripci�n al topic X
	SubscribedDev	= (1 << 5),//!< Suscrito al topic X
};


/** Estructura de datos de configuraci�n del cliente MQTT */
struct MQTTCfgData_t{
	unsigned char flags;
	uint8_t qos;
	uint16_t keep_alive;
	uint16_t group;
	uint16_t group_mask;
	uint32_t pollTimeout;
	char will_topic[MaxLengthOfMqttStrings];
	char will_message[MaxLengthOfMqttStrings];
	char username[MaxLengthOfMqttStrings];
	char password[MaxLengthOfMqttStrings];
	char address[MaxLengthOfMqttStrings];
};



}

#endif
