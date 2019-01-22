/* test_mean.c: Implementation of a testable component.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "MQTTClient.h"
#include "unity.h"
#include "WifiInterface.h"

static const char *TAG = "Test_MQTTClient";
static const char *URL = "192.168.254.79";
static const uint32_t PORT = 1883;
static const uint8_t MaxSizeOfAliasName = 22;
static FSManager* fs = NULL;
static MQTTClient* mqttcli = NULL;

#define P2P_PRODUCT_FAMILY			"XEO"
#define P2P_PRODUCT_TYPE			"PPL"
#define P2P_PRODUCT_SERIAL			"XEPPL00000000"

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
        
        mqttcli = new MQTTClient(root_topic, dev_name, P2P_PRODUCT_SERIAL, URL, PORT, fs, true);
        
        MBED_ASSERT(mqttcli);
        mqttcli->setPublicationBase("mqtt");
        mqttcli->setSubscriptionBase("mqtt");
        do{
            Thread::wait(100);
        }while(!mqttcli->ready());
        
        /*DEBUG_TRACE_I(_EXPR_, _MODULE_, "Instalando bridges mqlib <-> mqtt");
        mqttcli->addLocalBridge("stat/cfg/astcal");
        mqttcli->addLocalBridge("stat/cfg/energy");
        mqttcli->addLocalBridge("stat/value/energy");
        mqttcli->addLocalBridge("stat/cfg/light");
        mqttcli->addLocalBridge("stat/value/light");
        mqttcli->addLocalBridge("stat/boot/sys");*/
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
    Thread::wait(10);
	TEST_ASSERT_EQUAL(count_err, 0);
}

