#include "MQTTClient.h"

static const char* _MODULE_ = "[MqttCli].......";
#define _EXPR_	(!IS_ISR())


//------------------------------------------------------------------------------------
bool MQTTClient::checkIntegrity(){
	if(strlen(mqttLocalCfg.username) >= Blob::MaxLengthOfLoginStrings){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERROR: strlen(username) %d", strlen(mqttLocalCfg.username));
		return false;
	}
	if(strlen(mqttLocalCfg.passwd) >= Blob::MaxLengthOfLoginStrings){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERROR: strlen(passwd) %d", strlen(mqttLocalCfg.passwd));
		return false;
	}
	return true;
}


//------------------------------------------------------------------------------------
void MQTTClient::setDefaultConfig(){
	mqttLocalCfg.updFlagMask = Blob::EnableMqttCfgUpdNotif;
	mqttLocalCfg.groupMask = 0;
	mqttLocalCfg.keepAlive = 0;
	mqttLocalCfg.qos = 0;
	strncpy(mqttLocalCfg.username, (const char*)"", Blob::MaxLengthOfLoginStrings);
	strncpy(mqttLocalCfg.passwd, (const char*)"", Blob::MaxLengthOfLoginStrings);
	mqttLocalCfg.verbosity = ESP_LOG_VERBOSE;
	saveConfig();
}


//------------------------------------------------------------------------------------
void MQTTClient::restoreConfig(){

	uint32_t crc = 0;
	DEBUG_TRACE_I(_EXPR_, _MODULE_, "Recuperando datos de memoria NV...");
	bool success = true;
	if(!restoreParameter("MqttUpdFlags", &mqttLocalCfg.updFlagMask, sizeof(uint32_t), NVSInterface::TypeUint32)){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_NVS leyendo UpdFlags!");
		success = false;
	}
	if(!restoreParameter("MqttGroupMsk", &mqttLocalCfg.groupMask, sizeof(uint32_t), NVSInterface::TypeUint32)){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_NVS leyendo groupMask!");
		success = false;
	}
	if(!restoreParameter("MqttKeepAlive", &mqttLocalCfg.keepAlive, sizeof(uint32_t), NVSInterface::TypeUint32)){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_NVS leyendo keepAlive!");
		success = false;
	}
	if(!restoreParameter("MqttQos", &mqttLocalCfg.qos, sizeof(int32_t), NVSInterface::TypeInt32)){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_NVS leyendo qos!");
		success = false;
	}
	if(!restoreParameter("MqttUsername", mqttLocalCfg.username, Blob::MaxLengthOfLoginStrings, NVSInterface::TypeString)){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_NVS leyendo username!");
		success = false;
	}
	if(!restoreParameter("MqttPasswd", mqttLocalCfg.passwd, Blob::MaxLengthOfLoginStrings, NVSInterface::TypeString)){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_NVS leyendo passwd!");
		success = false;
	}
	if(!restoreParameter("MqttVerbosity", &mqttLocalCfg.verbosity, sizeof(esp_log_level_t), NVSInterface::TypeUint32)){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_NVS leyendo verbosity!");
		success = false;
	}
	if(!restoreParameter("MqttChecksum", &crc, sizeof(uint32_t), NVSInterface::TypeUint32)){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_NVS leyendo Checksum!");
		success = false;
	}

	if(success){
		// chequea el checksum crc32 y despu�s la integridad de los datos
		DEBUG_TRACE_I(_EXPR_, _MODULE_, "Datos recuperados. Chequeando integridad...");
		if(Blob::getCRC32(&mqttLocalCfg, sizeof(Blob::MQTTCfgData_t)) != crc){
			DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_CFG. Ha fallado el checksum");
		}
    	else if(!checkIntegrity()){
    		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_CFG. Ha fallado el check de integridad. Establece configuraci�n por defecto.");
    	}
    	else{
    		DEBUG_TRACE_D(_EXPR_, _MODULE_, "Check de integridad OK, activando verbose mode=%d", mqttLocalCfg.verbosity);
    		esp_log_level_set(_MODULE_, mqttLocalCfg.verbosity);
    		esp_log_level_set("MQTT_CLIENT", mqttLocalCfg.verbosity);
    		esp_log_level_set("TRANS_TCP", mqttLocalCfg.verbosity);
    		DEBUG_TRACE_D(_EXPR_, _MODULE_, "Verbosity = %d", mqttLocalCfg.verbosity);
    	}
	}
	else{
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_FS. Error en la recuperaci�n de datos. Establece configuraci�n por defecto");
		setDefaultConfig();
	}
}


//------------------------------------------------------------------------------------
void MQTTClient::saveConfig(){
	DEBUG_TRACE_I(_EXPR_, _MODULE_, "Guardando datos en memoria NV...");

	if(!saveParameter("MqttUpdFlags", &mqttLocalCfg.updFlagMask, sizeof(uint32_t), NVSInterface::TypeUint32)){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_NVS grabando UpdFlags!");
	}
	if(!saveParameter("MqttGroupMsk", &mqttLocalCfg.groupMask, sizeof(uint32_t), NVSInterface::TypeUint32)){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_NVS grabando groupMask!");
	}
	if(!saveParameter("MqttKeepAlive", &mqttLocalCfg.keepAlive, sizeof(uint32_t), NVSInterface::TypeUint32)){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_NVS grabando keepAlive!");
	}
	if(!saveParameter("MqttQos", &mqttLocalCfg.qos, sizeof(int32_t), NVSInterface::TypeInt32)){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_NVS grabando qos!");
	}
	if(!saveParameter("MqttUsername", mqttLocalCfg.username, Blob::MaxLengthOfLoginStrings, NVSInterface::TypeString)){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_NVS grabando username!");
	}
	if(!saveParameter("MqttPasswd", mqttLocalCfg.passwd, Blob::MaxLengthOfLoginStrings, NVSInterface::TypeString)){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_NVS grabando passwd!");
	}
	if(!saveParameter("MqttVerbosity", &mqttLocalCfg.verbosity, sizeof(esp_log_level_t), NVSInterface::TypeUint32)){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_NVS grabando verbosity!");
	}
	else{
		esp_log_level_set(_MODULE_, mqttLocalCfg.verbosity);
		esp_log_level_set("MQTT_CLIENT", mqttLocalCfg.verbosity);
		esp_log_level_set("TRANS_TCP", mqttLocalCfg.verbosity);
		DEBUG_TRACE_D(_EXPR_, _MODULE_, "Verbosity = %d", mqttLocalCfg.verbosity);
	}
	uint32_t crc = Blob::getCRC32(&mqttLocalCfg, sizeof(Blob::MQTTCfgData_t));
	if(!saveParameter("MqttChecksum", &crc, sizeof(uint32_t), NVSInterface::TypeUint32)){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_NVS grabando Checksum!");
	}
}


//------------------------------------------------------------------------------------
void MQTTClient::_updateConfig(const Blob::MQTTCfgData_t& cfg, uint32_t keys, Blob::ErrorData_t& err){
	err.code = Blob::ErrOK;
	if(keys == Blob::SysKeyNone){
		err.code = Blob::ErrEmptyContent;
		goto _updateConfigExit;
	}

	if(keys & Blob::MqttKeyCfgUpd){
		mqttLocalCfg.updFlagMask = cfg.updFlagMask;
	}
	if(keys & Blob::MqttKeyCfgKeepAlive){
		mqttLocalCfg.keepAlive = cfg.keepAlive;
	}
	if(keys & Blob::MqttKeyCfgGrpMsk){
		mqttLocalCfg.groupMask = cfg.groupMask;
	}
	if(keys & Blob::MqttKeyCfgQos){
		mqttLocalCfg.qos = cfg.qos;
	}
	if(keys & Blob::MqttKeyCfgVerbosity){
		mqttLocalCfg.verbosity = cfg.verbosity;
	}

	if(keys & Blob::MqttKeyCfgUsername){
		strncpy(mqttLocalCfg.username, cfg.username, Blob::MaxLengthOfLoginStrings);
	}
	if(keys & Blob::MqttKeyCfgPasswd){
		strncpy(mqttLocalCfg.passwd, cfg.passwd, Blob::MaxLengthOfLoginStrings);
	}

_updateConfigExit:
	strcpy(err.descr, Blob::errList[err.code]);
}
