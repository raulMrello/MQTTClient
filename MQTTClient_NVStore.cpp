#include "MQTTClient.h"

static const char* _MODULE_ = "[MqttCli].......";
#define _EXPR_	(!IS_ISR())


//------------------------------------------------------------------------------------
bool MQTTClient::checkIntegrity(){
	if(_mqtt_man.cfg.nvs_id != APP_MQTTCLIENT_NVS_ID){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERROR nvs_id=%d", _mqtt_man.cfg.nvs_id);
		return false;
	}
	if(strlen(_mqtt_man.cfg.mqttUrl) == 0 || strlen(_mqtt_man.cfg.mqttUrl) >= Blob::MaxLengthOfMqttStrings){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERROR strlen(mqttUrl) %d", strlen(_mqtt_man.cfg.mqttUrl));
		return false;
	}
	if(strlen(_mqtt_man.cfg.mqttUser) == 0 || strlen(_mqtt_man.cfg.mqttUser) >= Blob::MaxLengthOfUserLength){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERROR strlen(mqttUser) %d", strlen(_mqtt_man.cfg.mqttUser));
		return false;
	}
	if(strlen(_mqtt_man.cfg.mqttPass) == 0 || strlen(_mqtt_man.cfg.mqttPass) >= Blob::MaxLengthOfPassLength){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERROR strlen(mqttPass) %d", strlen(_mqtt_man.cfg.mqttPass));
		return false;
	}
	if(_mqtt_man.cfg.mqttPort == 0){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERROR mqttPort is 0");
		return false;
	}
	return true;
}


//------------------------------------------------------------------------------------
void MQTTClient::setDefaultConfig(){
	_mqtt_man.uid = UID_MQTT_MANAGER;
	_mqtt_man.cfg.updFlagMask = Blob::EnableMqttCfgUpdNotif;
	_mqtt_man.cfg.groupMask = 0;
	_mqtt_man.cfg.keepAlive = 0;
	_mqtt_man.cfg.qos = 0;
	strncpy(_mqtt_man.cfg.mqttUrl, MQTT_URL, Blob::MaxLengthOfMqttStrings);
	strncpy(_mqtt_man.cfg.mqttUser, MQTT_USER, Blob::MaxLengthOfUserLength);
	strncpy(_mqtt_man.cfg.mqttPass, MQTT_PASS, Blob::MaxLengthOfPassLength);
	_mqtt_man.cfg.mqttPort = MQTT_PORT;
	_mqtt_man.cfg.verbosity = APP_MQTTCLIENT_LOG_LEVEL;
	_mqtt_man.cfg.nvs_id = APP_MQTTCLIENT_NVS_ID;
	saveConfig();
}


//------------------------------------------------------------------------------------
void MQTTClient::restoreConfig(){

	uint32_t crc = 0;
	_mqtt_man.uid = UID_MQTT_MANAGER;
	DEBUG_TRACE_D(_EXPR_, _MODULE_, "Recuperando datos de memoria NV...");
	bool success = true;
	if(!restoreParameter("MqttCfg", &_mqtt_man.cfg, sizeof(mqtt_manager_cfg), NVSInterface::TypeBlob)){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_NVS leyendo mqttCfg!");
		success = false;
	}

	if(success){
    	if(!checkIntegrity()){
    		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_CFG. Ha fallado el check de integridad. Establece configuraci�n por defecto.");
    	}
    	else{
    		DEBUG_TRACE_D(_EXPR_, _MODULE_, "Check de integridad OK, activando verbose mode=%d", _mqtt_man.cfg.verbosity);
    		esp_log_level_set(_MODULE_, _mqtt_man.cfg.verbosity);
    		esp_log_level_set("MQTT_CLIENT", _mqtt_man.cfg.verbosity);
    		esp_log_level_set("TRANS_TCP", _mqtt_man.cfg.verbosity);
    		DEBUG_TRACE_D(_EXPR_, _MODULE_, "Verbosity = %d", _mqtt_man.cfg.verbosity);
    		return;
    	}
	}
	DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_FS. Error en la recuperaci�n de datos. Establece configuraci�n por defecto");
	setDefaultConfig();
}


//------------------------------------------------------------------------------------
void MQTTClient::saveConfig(){
	DEBUG_TRACE_D(_EXPR_, _MODULE_, "Guardando datos en memoria NV...");

	if(!saveParameter("MqttCfg", &_mqtt_man.cfg, sizeof(mqtt_manager_cfg), NVSInterface::TypeBlob)){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_NVS grabando mqttCfg!");
	}
	else{
		esp_log_level_set(_MODULE_, _mqtt_man.cfg.verbosity);
		esp_log_level_set("MQTT_CLIENT", _mqtt_man.cfg.verbosity);
		esp_log_level_set("TRANS_TCP", _mqtt_man.cfg.verbosity);
		DEBUG_TRACE_D(_EXPR_, _MODULE_, "Verbosity = %d", _mqtt_man.cfg.verbosity);
	}
}


//------------------------------------------------------------------------------------
void MQTTClient::_updateConfig(const mqtt_manager& data, Blob::ErrorData_t& err){
	err.code = Blob::ErrOK;

	if(data.cfg._keys & Blob::MqttKeyCfgUpd){
		_mqtt_man.cfg.updFlagMask = data.cfg.updFlagMask;
	}
	if(data.cfg._keys & Blob::MqttKeyCfgKeepAlive){
		_mqtt_man.cfg.keepAlive = data.cfg.keepAlive;
	}
	if(data.cfg._keys & Blob::MqttKeyCfgGrpMsk){
		_mqtt_man.cfg.groupMask = data.cfg.groupMask;
	}
	if(data.cfg._keys & Blob::MqttKeyCfgQos){
		_mqtt_man.cfg.qos = data.cfg.qos;
	}
	if(data.cfg._keys & Blob::MqttKeyCfgVerbosity){
		_mqtt_man.cfg.verbosity = data.cfg.verbosity;
	}

	if(data.cfg._keys & Blob::MqttKeyCfgUrl){
		strncpy(_mqtt_man.cfg.mqttUrl, data.cfg.mqttUrl, Blob::MaxLengthOfMqttStrings);
	}
	if(data.cfg._keys & Blob::MqttKeyCfgPort){
		_mqtt_man.cfg.mqttPort = data.cfg.mqttPort;
	}
	if(data.cfg._keys & Blob::MqttKeyCfgUsername){
		strncpy(_mqtt_man.cfg.mqttUser, data.cfg.mqttUser, Blob::MaxLengthOfUserLength);
	}
	if(data.cfg._keys & Blob::MqttKeyCfgPasswd){
		strncpy(_mqtt_man.cfg.mqttPass, data.cfg.mqttPass, Blob::MaxLengthOfPassLength);
	}

	strcpy(err.descr, Blob::errList[err.code]);
}
