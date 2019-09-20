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
#define P2P_PRODUCT_TYPE			"PPL"
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
 * @brief Test para verificar el inicio de los interfaces wifi
 */
TEST_CASE("TEST_WIFI_START", "[MQTTClient]")
{
    MDF_LOGI("Iniciando TEST 1");
	esp_log_level_set(TAG, ESP_LOG_DEBUG);
	mdf_err_t result = WifiInterface::start("Wifi", CONFIG_ROUTER_SSID, CONFIG_ROUTER_PASSWORD, CONFIG_MESH_ID, CONFIG_MESH_PASSWORD);
	TEST_ASSERT_EQUAL(result, MDF_OK);
}


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

class test_wifi_registration{
public:
	test_wifi_registration(){
		_handled_count = 0;
		_unhandled_count = 0;
		_evlist.push_back(MDF_EVENT_MWIFI_ROOT_GOT_IP);
		_evlist.push_back(MDF_EVENT_MWIFI_ROOT_LOST_IP);
	}

	mdf_err_t test_run(){
		return WifiInterface::attachEventHandler((uint32_t)this, callback(this, &test_wifi_registration::_eventLoopCb), _evlist);
	}

	void getCounters(int& handled, int& unhandled){
		handled = _handled_count; unhandled = _unhandled_count;
	}

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
		mqttcli->addServerBridge("stat/+/+/value/light");
		mqttcli->addServerBridge("stat/+/+/value/astcal");
	    mqttcli->addServerBridge("stat/+/+/cfg/+");
	    mqttcli->addServerBridge("stat/+/+/+/fwupd");

		mqttcli->init(root_topic, "1234567890", "123456");

		MDF_LOGI("MQTTClient OK!");
    }

private:
    std::list<mdf_event_loop_t> _evlist;
	mdf_err_t _eventLoopCb(mdf_event_loop_t event, void* ctx){
		switch(event){
			case MDF_EVENT_MWIFI_ROOT_GOT_IP: {
				MDF_LOGI("TEST, Got IP");
				_handled_count++;
                MDF_LOGI("Iniciando MQTTClient");
                initMQTTClient();
                MDF_LOGI("MQTT iniciado");
				break;
			}

			case MDF_EVENT_MWIFI_ROOT_LOST_IP: {
				MDF_LOGI("TEST, Lost IP");
				_handled_count++;
				break;
			}

			default:
				_unhandled_count++;
				break;
		}
		return MDF_OK;
	}

	int _handled_count;
	int _unhandled_count;
};


TEST_CASE("TEST_WIFI_REGISTRATION", "[MQTTClient]")
{
	esp_log_level_set(TAG, ESP_LOG_DEBUG);

	test_wifi_registration *t = new test_wifi_registration();

	// checks registration
	mdf_err_t result = t->test_run();
	TEST_ASSERT_EQUAL(result, MDF_OK);

	// checks event results
	int count_ok=0,count_err=0;
	while((count_ok+count_err)==0){
		Thread::wait(1);
		t->getCounters(count_ok, count_err);
	}
    Thread::wait(10000);
	TEST_ASSERT_EQUAL(count_err, 0);
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

	struct __packed LightStatData_t{
		uint32_t flags;
		uint8_t outValue;
	};
	LightStatData_t light = {1,5};

	MQ::PublishCallback pubMQTT = callback(publicationCb);

	MQ::MQClient::publish("stat/0/XEPPL00000000/value/light", &light, sizeof(LightStatData_t), &pubMQTT);

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