#ifndef __MQTTCLIENT__H
#define __MQTTCLIENT__H

#include "mbed.h"
#include "ActiveModule.h"
#include "MQTTClientBlob.h"
#include "mqtt_client.h"

class MQTTClient : public ActiveModule {
    public:
        /** Constructor por defecto
         *  @param root_topic Root del topic base de escucha
         *  @param client_id Identificador del cliente
         * 	@param fs Objeto FSManager para operaciones de backup
         * 	@param defdbg Flag para habilitar depuración por defecto
         */
        MQTTClient(const char* root_topic, const char* network_id, const char* client_id, const char *uri, uint32_t port, FSManager* fs, bool defdbg = false);

        virtual ~MQTTClient(){}
    
    private:
        /** M�ximo n�mero de mensajes alojables en la cola asociada a la m�quina de estados */
        static const uint32_t MaxQueueMessages = 16;

        /** Máximo número de topics a los que puede estar suscrito en remoto */
        static const uint8_t MaxSubscribedTopics = 2;

        /** Parámetros de conexión estáticos: topics de dispositivo y grupo, id de la red y UID del nodo */
        char _root_topic[Blob::MaxLengthOfMqttStrings];
        char _network_id[Blob::MaxLengthOfMqttStrings];
        char _client_id[Blob::MaxLengthOfMqttStrings];
        char _subsc_topic[MaxSubscribedTopics][Blob::MaxLengthOfMqttStrings];

        /** Contador de mensajes enviados */
        uint16_t _msg_counter;

        /* Configuración de cliente mqtt */
        esp_mqtt_client_config_t mqtt_cfg;

        /* Manejador del cliente mqtt */
        esp_mqtt_client_handle_t clientHandle;

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
            MqttUnHndEvt	= (State::EV_RESERVED_USER << 16),	/// <--- Fin flags MQTT
            RecvCfgSet		= (State::EV_RESERVED_USER << 17),	/// Flag al cambiar la configuraci�n del dispositivo 'cmd/mqtt/cfg/set'
            FwdMsgLocalEvt	= (State::EV_RESERVED_USER << 18),	/// Flag al solicitar un reenv�o hacia el broker mqtt
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
            char* data;
            uint16_t data_len;
        };

        /** Cola de mensajes de la m�quina de estados */
        Queue<State::Msg, MaxQueueMessages> _queue;

        //Establece los valores de configuración para conectar con el servidor MQTT
        void setConfigMQTTServer(const char*, uint32_t);

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

        /** Notifica localmente un cambio de estado
         *
         */
        void notifyConnStatUpdate();

        esp_err_t mqtt_EventHandler(esp_mqtt_event_handle_t event);
};

#endif /*__MQTTCLIENT__H */