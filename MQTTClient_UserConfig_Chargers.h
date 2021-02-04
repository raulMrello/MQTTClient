/*
 * MQTTClient_UserCfg
*
 *  Created on: Oct 2019
 *      Author: raulMrello
 *
 *	Este archivo de cabecera incluye todas las definiciones necesarias para que el componente MQTTClient
 *	sea capaz de activar los diferentes subcomponentes que lo forman, tales como selección de pines I/O
 *	CPU en la que se ejecutan los subcomponentes, prioridad de la tarea, stack_size asociado, etc...
 */
 
#ifndef __MQTTClient_UserCfg_Chargers__H
#define __MQTTClient_UserCfg_Chargers__H

#include "mbed.h"

//------------------------------------------------------------------------------------
//--- TOPIC CONFIG ------------------------------------------------------------
//------------------------------------------------------------------------------------

#define MQTT_URL		"188.208.218.221"
#define MQTT_USER		"VIARIS_uni"
#define MQTT_PASS		"dhbu-9i3assdv2489"
#define MQTT_PORT		1883

#endif
