/* test_mean.c: Implementation of a testable component.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "MQTTClient.h"
#include "unity.h"
#include "WifiInterface.h"

#define P2P_PRODUCT_FAMILY			"XEO"
#define P2P_PRODUCT_TYPE			"MOD"
#define P2P_PRODUCT_SERIAL			"XEPPL00000000"

static const char *URL = "192.168.254.65";
static const uint32_t PORT = 1883;
static const char *TAG = "Test_MQTTClient";
static const uint8_t MaxSizeOfAliasName = 22;
static FSManager* fs = NULL;
static MQTTClient* mqttcli = NULL;

/** Objeto para habilitar trazas de depuraci�n syslog */
void (*syslog_print)(const char*level, const char* tag, const char* format, ...) = NULL;


//---------------------------------------------------------------------------
/**
 * @brief Test para verificar el registro de un componente externo. Se suscribe
 * a eventos relativos a obtenci�n y p�rdida de IP
 */
#include<list>
#include "mdf_common.h"
#include "mwifi.h"
#include "mbed.h"
#include "FSManager.h"



void initMQTTClient()
{
	/**************** MQLib Setup **************/

	// Arranca el broker con la siguiente configuraci�n:
	//  - Lista de tokens predefinida
	//  - N�mero m�ximo de caracteres para los topics: 64 caracteres incluyendo fin de cadena '\0'
	MDF_LOGI("Iniciando MQLib... ");
	MQ::MQBroker::start(64, true);

	// Espera a que el broker est� operativo
	while(!MQ::MQBroker::ready()){
		Thread::wait(100);
	}
	MDF_LOGI("MQLib OK!");


	MDF_LOGI("Iniciando FSManager");
	fs = new FSManager("fs");
	if(fs->ready()){
		#warning TODO Recuperar datos de backup
		// ... a�adir c�digo aqu�
	}
	MDF_LOGI("FSManager OK!");
	
	char dev_name[MaxSizeOfAliasName];
	strcpy(dev_name, P2P_PRODUCT_SERIAL);
	
	char root_topic[64];
	sprintf(root_topic, "%s/%s", P2P_PRODUCT_FAMILY, P2P_PRODUCT_TYPE);
	MDF_LOGI("Iniciando cliente MQTT en root_topic %s", root_topic);
	
	mqttcli = new MQTTClient(fs, true);
	MBED_ASSERT(mqttcli);
	mqttcli->setJSONSupport(false);
	mqttcli->setPublicationBase("mqtt");
	mqttcli->setSubscriptionBase("mqtt");
	do{
		Thread::wait(100);
	}while(!mqttcli->ready());
	
	MDF_LOGI("Instalando bridges mqlib <-> mqtt");
	mqttcli->addServerBridge("stat/+/+/boot/sys");
	mqttcli->addServerBridge("stat/+/+/value/sys");
	mqttcli->addServerBridge("stat/+/+/modules/sys");
	mqttcli->addServerBridge("stat/+/+/value/energy");
	mqttcli->addServerBridge("stat/+/+/value/reqman");
	mqttcli->addServerBridge("stat/+/+/value/astcal");
	mqttcli->addServerBridge("stat/+/+/cfg/+");
	mqttcli->addServerBridge("stat/+/+/+/fwupd");

	mqttcli->init(root_topic, "1234567890", "123456");

	MDF_LOGI("MQTTClient OK!");
}

TEST_CASE("TEST_WIFI_START", "[MQTTClient]")
{
    MDF_LOGI("Iniciando TEST 1");
	WifiInterface::wifiInit();
	MDF_LOGI("Arrancando WifiInterface...");
	mdf_err_t result = WifiInterface::start("Wifi", "Invitado", "11FF00DECA", "123456", "111222111");
	while(!WifiInterface::hasIp()){
		Thread::wait(100);
	}
	MDF_LOGI("WifiInterface arrancado OK!. HEAP_8=%d, HEAP_32=%d",

				  heap_caps_get_free_size(MALLOC_CAP_8BIT),
				  heap_caps_get_free_size(MALLOC_CAP_32BIT));

	initMQTTClient();

	TEST_ASSERT_EQUAL(result, MDF_OK);
}



bool testFin = false;

void publicationCb(const char* topic, int32_t result)
{
	MDF_LOGI("PUB_DONE en topic local '%s' con resultado '%d'", topic, result);
	testFin = true;
}

TEST_CASE("TEST_MQTTClient_PUBLISH", "[MQTTClient]")
{
	esp_log_level_set(TAG, ESP_LOG_DEBUG);

	struct __packed element_request{
		uint32_t uid;
		char source[MaxSourceLength];         //uid del modulo
		uint8_t priority;       // 0 máxima prioridad
		uint8_t action;
		//uint8_t stat;
		uint32_t idUser;
		uint32_t idGroup;
	};

	element_request bla = {};
	bla.uid = 1;
	strcpy(bla.source,"mennekes");
	bla.source[strlen("mennekes")]=0;
	bla.priority = 0;
	bla.action = 1;
	bla.idUser = 22123;
	bla.idGroup = 0;

	Blob::NotificationData_t<element_request> *notif = new Blob::NotificationData_t<element_request>(bla);
	//JsonParser::printBinaryObject(topic, notif, sizeof(Blob::NotificationData_t<element_request>));
	delete(notif);

	MQ::PublishCallback pubMQTT = callback(publicationCb);

	MQ::MQClient::publish("stat/0/XEPPL00000000/value/reqman", notif, sizeof(Blob::NotificationData_t<element_request>), &pubMQTT);

	while(!testFin)
	{
		Thread::wait(1);
	}
	Thread::wait(2000);
	TEST_ASSERT_EQUAL(1, MDF_OK);
}


/*TEST_CASE("TEST_MQTTClient_SUBSCRIBER", "[MQTTClient]")
{
	esp_log_level_set(TAG, ESP_LOG_DEBUG);

	MQ::MQClient::publish("stat/value/light", &light, sizeof(Blob::LightStatData_t), &pubMQTT);

	while(!testFin)
	{
		Thread::wait(1);
	}
	Thread::wait(2000);
	TEST_ASSERT_EQUAL(1, MDF_OK);
}*/