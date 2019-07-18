#ifndef __MQTTCLIENT__H
#define __MQTTCLIENT__H

#include "mbed.h"
#include "ActiveModule.h"
#include "MQTTClientBlob.h"
#include "mqtt_client.h"
#include "JsonParserBlob.h"
#include <map>
#include <mwifi.h>
#include <algorithm>
/** Flag para habilitar el soporte de objetos JSON en las suscripciones a MQLib
 *  Por defecto DESACTIVADO
 */
#define MQTT_ENABLE_JSON_SUPPORT		0



class MQTTClient : public ActiveModule {
public:
	/** Constructor por defecto
	 *  @param rootTopic Root del topic base de escucha
	 *  @param clientId Identificador del cliente
	 * 	@param fs Objeto FSManager para operaciones de backup
	 * 	@param defdbg Flag para habilitar depuración por defecto
	 */
	MQTTClient(FSManager* fs, bool defdbg = false);

	void init(const char* rootTopic, const char* clientId, const char* networkId);

	bool setConfig(const char *host, uint32_t port, const char *user, const char *pass, bool restart);

	void addServerBridge(char* topic);
	void removeServerBridge(char* topic);

	void start();
	void stop();

	/** Notifica localmente un cambio de estado
	 *
	 */
	void notifyConnStatUpdate();

	virtual ~MQTTClient(){}


	/**
	 * Activa y/o desactiva el soporte JSON
	 * @param flag
	 */
	void setJSONSupport(bool flag){
		_json_supported = flag;
	}


	/**
	 * Obtiene el estado del soporte JSON
	 * @return
	 */
	bool isJSONSupported(){
		return _json_supported;
	}

private:
	/** M�ximo n�mero de mensajes alojables en la cola asociada a la m�quina de estados */
	static const uint32_t MaxQueueMessages = 16;

	/** Máximo número de topics a los que puede estar suscrito en remoto */
	static const uint8_t MaxSubscribedTopics = 2;

	/** Parámetros de conexión estáticos: topics de dispositivo y grupo, id de la red y UID del nodo */
	char rootNetworkTopic[Blob::MaxLengthOfMqttStrings];
	char clientId[Blob::MaxLengthOfMqttStrings];
	char subscTopic[MaxSubscribedTopics][Blob::MaxLengthOfMqttStrings];

	/* Mapa para comprobar si la conexión de los subscriptores se ha hecho correctamente*/
	std::map<int, bool> topicsSubscribed;

	/** Contador de mensajes enviados */
	uint16_t msgCounter;

	/* Configuración de cliente mqtt */
	esp_mqtt_client_config_t mqttCfg;
	mqtt_manager _mqtt_man;
	std::vector<char *> _serverBridges;

	/** Flag de control para el soporte de objetos json */
	bool _json_supported;

	/* Flag para comunicar estado */
	bool isConnected;

	/* Manejador del cliente mqtt */
	esp_mqtt_client_handle_t clientHandle;

	Callback<void(const char* name, void*, uint16_t)> subscriptionToServerCb;

	/** Flags de operaciones a realizar por la tarea */
	enum MsgEventFlags{
		WifiUpEvt 		= (State::EV_RESERVED_USER << 0),  	/// Flag activado al estar la wifi levantada (con IP)
		WifiDownEvt	 	= (State::EV_RESERVED_USER << 1),  	/// Flag activado al caerse la wifi
		MqttConnEvt		= (State::EV_RESERVED_USER << 2),   /// Flags MQTT --->
		MqttConnAckEvt	= (State::EV_RESERVED_USER << 3),
		MqttPublEvt		= (State::EV_RESERVED_USER << 4),
		MqttPubAckEvt	= (State::EV_RESERVED_USER << 5),
		MqttPubRelEvt	= (State::EV_RESERVED_USER << 6),
		MqttPubCompEvt	= (State::EV_RESERVED_USER << 7),
		MqttSubscrEvt	= (State::EV_RESERVED_USER << 8),
		MqttSubAckEvt	= (State::EV_RESERVED_USER << 9),
		MqttUnsubscrEvt	= (State::EV_RESERVED_USER << 10),
		MqttUnsubAckEvt	= (State::EV_RESERVED_USER << 11),
		MqttPingReqEvt	= (State::EV_RESERVED_USER << 12),
		MqttPingRespEvt	= (State::EV_RESERVED_USER << 13),
		MqttDiscEvt		= (State::EV_RESERVED_USER << 14),
		MqttErrorEvt	= (State::EV_RESERVED_USER << 15),
		MqttUnHndEvt	= (State::EV_RESERVED_USER << 16),
		MqttDataEvt 	= (State::EV_RESERVED_USER << 17),
		MqttPublishToServer = (State::EV_RESERVED_USER << 18),	/// <--- Fin flags MQTT
		RecvCfgSet		= (State::EV_RESERVED_USER << 19),	/// Flag al cambiar la configuraci�n del dispositivo 'set/cfg/mqtt'
		FwdMsgLocalEvt	= (State::EV_RESERVED_USER << 20),	/// Flag al solicitar un reenv�o hacia el broker mqtt
		RecvCfgGet		= (State::EV_RESERVED_USER << 21),	/// Flag al solicitar la configuraci�n del dispositivo 'get/cfg/mqtt'
		//-------------
		UNKNOWN_EVENT	= (State::EV_RESERVED_USER << 31),	/// Flag desconocido
	};

	/** Estructura de los mensajes generados al recibir un evento mongoose
	 * 	@var nc Conexi�n mongoose
	 * 	@var data Puntero a datos. El puntero puede contener alg�n dato (uint32_t)
	 */

	struct MqttEvtMsg_t {
		void *data;
	};


	/** Estructura que incluye informaci�n sobre un topic y los datos recibidos asociados al mismo
	 * 	@var topic Nombre del topic
	 * 	@var data Datos asociados al topic (mensaje)
	 * 	@var data_len Tama�o en bytes de los datos asociados (mensaje)
	 */
	struct MqttTopicData_t{
		char topic[Blob::MaxLengthOfMqttStrings];
		void* data;
		uint16_t data_len;
	};

	struct MqttMsg_t{
		char* topic;
		void *data;
		uint16_t topic_len;
		uint16_t data_len;
	};

	struct MeshTopicData_t{
		char topic[Blob::MaxLengthOfMqttStrings];
		void* data;
	};

	/** Cola de mensajes de la m�quina de estados */
	Queue<State::Msg, MaxQueueMessages> queueSM;

	//Establece los valores de configuración para conectar con el servidor MQTT
	void setConfigMQTTServer(const char*, uint32_t, const char*, const char*);

	/** Callback invocada al recibir una actualización de un topic local al que está suscrito
	 *  @param topic Identificador del topic
	 *  @param msg Mensaje recibido
	 *  @param msg_len Tamaño del mensaje
	 */
	virtual void subscriptionCb(const char* topic, void* msg, uint16_t msg_len);


	/** Callback invocada al finalizar una publicación local
	 *  @param topic Identificador del topic
	 *  @param result Resultado de la publicación
	 */
	virtual void publicationCb(const char* topic, int32_t result);

	/** Interfaz para manejar los eventos en la máquina de estados por defecto
	 *  @param se Evento a manejar
	 *  @return State::StateResult Resultado del manejo del evento
	 */
	virtual State::StateResult Init_EventHandler(State::StateEvent* se);

	/** Interfaz para postear un mensaje de la m�quina de estados en el Mailbox de la clase heredera
	 *  @param msg Mensaje a postear
	 *  @return Resultado
	 */
	virtual osStatus putMessage(State::Msg *msg);

	/** Interfaz para obtener un evento osEvent de la clase heredera
	 *  @param msg Mensaje a postear
	 */
	virtual osEvent getOsEvent();

	/** Chequea la integridad de los datos de configuraci�n <_cfg>. En caso de que algo no sea
	 * 	coherente, restaura a los valores por defecto y graba en memoria NV.
	 * 	@return True si la integridad es correcta, False si es incorrecta
	 */
	virtual bool checkIntegrity();


	/** Establece la configuraci�n por defecto grab�ndola en memoria NV
	 */
	virtual void setDefaultConfig();

	/** Recupera la configuraci�n de memoria NV
	 */
	virtual void restoreConfig();


	/** Graba la configuraci�n en memoria NV
	 */
	virtual void saveConfig();


	/** Actualiza la configuracion
	 *
	 * @param data Nueva configuraci�n a aplicar
	 * @param err Recibe los errores generados durante la actualizaci�n
	 */
	void _updateConfig(const mqtt_manager& data, Blob::ErrorData_t& err);

	void parseMqttTopic(char* local_topic, const char* mqtt_topic);

	void parseLocalTopic(char* mqtt_topic, const char* local_topic);

	bool getRelativeTopic(char* relativeTopic, const char* localTopic);
	bool getRelativeTopic(char* relativeTopic, const char* localTopic, bool* isOwn);

	void subscrToServerCb(const char* topic, void* msg, uint16_t msg_len);

	bool checkServerBridge(char* topic);


	Callback<int32_t(const char*, void *, uint32_t,
		Callback<void(const char*, int32_t)> *)> myPublisher;

	int32_t publish(const char* topic, void *data, uint32_t data_size,
		Callback<void(const char*, int32_t)> *publisher);

	esp_err_t mqtt_EventHandler(esp_mqtt_event_handle_t event);
};

#endif /*__MQTTCLIENT__H */
