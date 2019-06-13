#include "MQTTClient.h"

static const char* _MODULE_ = "[MqttCli].......";
#define _EXPR_	(!IS_ISR())


//------------------------------------------------------------------------------------
bool MQTTClient::checkIntegrity(){
	if(strlen(_mqtt_man.cfg.username) >= Blob::MaxLengthOfLoginStrings){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERROR: strlen(username) %d", strlen(_mqtt_man.cfg.username));
		return false;
	}
	if(strlen(_mqtt_man.cfg.passwd) >= Blob::MaxLengthOfLoginStrings){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERROR: strlen(passwd) %d", strlen(_mqtt_man.cfg.passwd));
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
	strncpy(_mqtt_man.cfg.username, (const char*)"", Blob::MaxLengthOfLoginStrings);
	strncpy(_mqtt_man.cfg.passwd, (const char*)"", Blob::MaxLengthOfLoginStrings);
	_mqtt_man.cfg.verbosity = ESP_LOG_VERBOSE;
	saveConfig();
}


//------------------------------------------------------------------------------------
void MQTTClient::restoreConfig(){

	uint32_t crc = 0;
	_mqtt_man.uid = UID_MQTT_MANAGER;
	DEBUG_TRACE_I(_EXPR_, _MODULE_, "Recuperando datos de memoria NV...");
	bool success = true;
	if(!restoreParameter("MqttUpdFlags", &_mqtt_man.cfg.updFlagMask, sizeof(uint32_t), NVSInterface::TypeUint32)){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_NVS leyendo UpdFlags!");
		success = false;
	}
	if(!restoreParameter("MqttGroupMsk", &_mqtt_man.cfg.groupMask, sizeof(uint32_t), NVSInterface::TypeUint32)){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_NVS leyendo groupMask!");
		success = false;
	}
	if(!restoreParameter("MqttKeepAlive", &_mqtt_man.cfg.keepAlive, sizeof(uint32_t), NVSInterface::TypeUint32)){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_NVS leyendo keepAlive!");
		success = false;
	}
	if(!restoreParameter("MqttQos", &_mqtt_man.cfg.qos, sizeof(int32_t), NVSInterface::TypeInt32)){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_NVS leyendo qos!");
		success = false;
	}
	if(!restoreParameter("MqttUsername", _mqtt_man.cfg.username, Blob::MaxLengthOfLoginStrings, NVSInterface::TypeString)){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_NVS leyendo username!");
		success = false;
	}
	if(!restoreParameter("MqttPasswd", _mqtt_man.cfg.passwd, Blob::MaxLengthOfLoginStrings, NVSInterface::TypeString)){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_NVS leyendo passwd!");
		success = false;
	}
	if(!restoreParameter("MqttVerbosity", &_mqtt_man.cfg.verbosity, sizeof(esp_log_level_t), NVSInterface::TypeUint32)){
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
		if(Blob::getCRC32(&_mqtt_man.cfg, sizeof(Blob::MQTTCfgData_t)) != crc){
			DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_CFG. Ha fallado el checksum");
		}
    	else if(!checkIntegrity()){
    		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_CFG. Ha fallado el check de integridad. Establece configuraci�n por defecto.");
    	}
    	else{
    		DEBUG_TRACE_D(_EXPR_, _MODULE_, "Check de integridad OK, activando verbose mode=%d", _mqtt_man.cfg.verbosity);
    		esp_log_level_set(_MODULE_, _mqtt_man.cfg.verbosity);
    		esp_log_level_set("MQTT_CLIENT", _mqtt_man.cfg.verbosity);
    		esp_log_level_set("TRANS_TCP", _mqtt_man.cfg.verbosity);
    		DEBUG_TRACE_D(_EXPR_, _MODULE_, "Verbosity = %d", _mqtt_man.cfg.verbosity);
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

	if(!saveParameter("MqttUpdFlags", &_mqtt_man.cfg.updFlagMask, sizeof(uint32_t), NVSInterface::TypeUint32)){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_NVS grabando UpdFlags!");
	}
	if(!saveParameter("MqttGroupMsk", &_mqtt_man.cfg.groupMask, sizeof(uint32_t), NVSInterface::TypeUint32)){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_NVS grabando groupMask!");
	}
	if(!saveParameter("MqttKeepAlive", &_mqtt_man.cfg.keepAlive, sizeof(uint32_t), NVSInterface::TypeUint32)){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_NVS grabando keepAlive!");
	}
	if(!saveParameter("MqttQos", &_mqtt_man.cfg.qos, sizeof(int32_t), NVSInterface::TypeInt32)){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_NVS grabando qos!");
	}
	if(!saveParameter("MqttUsername", _mqtt_man.cfg.username, Blob::MaxLengthOfLoginStrings, NVSInterface::TypeString)){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_NVS grabando username!");
	}
	if(!saveParameter("MqttPasswd", _mqtt_man.cfg.passwd, Blob::MaxLengthOfLoginStrings, NVSInterface::TypeString)){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_NVS grabando passwd!");
	}
	if(!saveParameter("MqttVerbosity", &_mqtt_man.cfg.verbosity, sizeof(esp_log_level_t), NVSInterface::TypeUint32)){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_NVS grabando verbosity!");
	}
	else{
		esp_log_level_set(_MODULE_, _mqtt_man.cfg.verbosity);
		esp_log_level_set("MQTT_CLIENT", _mqtt_man.cfg.verbosity);
		esp_log_level_set("TRANS_TCP", _mqtt_man.cfg.verbosity);
		DEBUG_TRACE_D(_EXPR_, _MODULE_, "Verbosity = %d", _mqtt_man.cfg.verbosity);
	}
	uint32_t crc = Blob::getCRC32(&_mqtt_man.cfg, sizeof(Blob::MQTTCfgData_t));
	if(!saveParameter("MqttChecksum", &crc, sizeof(uint32_t), NVSInterface::TypeUint32)){
		DEBUG_TRACE_W(_EXPR_, _MODULE_, "ERR_NVS grabando Checksum!");
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

	if(data.cfg._keys & Blob::MqttKeyCfgUsername){
		strncpy(_mqtt_man.cfg.username, data.cfg.username, Blob::MaxLengthOfLoginStrings);
	}
	if(data.cfg._keys & Blob::MqttKeyCfgPasswd){
		strncpy(_mqtt_man.cfg.passwd, data.cfg.passwd, Blob::MaxLengthOfLoginStrings);
	}

	strcpy(err.descr, Blob::errList[err.code]);
}
