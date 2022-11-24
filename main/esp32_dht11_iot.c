/* 
Main reference: https://www.mouser.com/datasheet/2/758/DHT11-Technical-Data-Sheet-Translated-Version-1143054.pdf
Basic structure:
1. MCU sends out start signal to dht11 by pulling down voltage for at least 18 ms
2. MCU pulls up voltage and waits for dht11 to respond (20-40ms)
3. DHT11 sends out low response signal for 80 us, then pulls up for 80us and readies for data transmission
4. Data transmission is sent, total of 40 bits or 5 bytes. Data format: [int rh, float rh, int temp, float temp, checksum]

*/

#include <stdio.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include <string.h>
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include <esp_http_server.h>
#include "nvs_flash.h"
#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include <sys/param.h>

//PINS
#define DHT11_PIN     4
#define BLUELED_PIN 16

//WIFI
#define EXAMPLE_ESP_WIFI_SSID CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY  20
/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define BUFFERSIZE 2048

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;
static const char *TAG = "wifi station";
static int s_retry_num = 0;

struct data{
    int temperature,humidity,status;
};

/*WIFI setup section START*/
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	     .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s",
                 EXAMPLE_ESP_WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
}
/*WIFI setup section END*/


/*DHT11 section START*/

/*MCU sends out start signal to dht and dht responds*/
void startSignal(void)
{ 
    //set pin to ouput, pull down for at least 18 ms to let dht11 detect signal
    gpio_set_direction(DHT11_PIN, GPIO_MODE_OUTPUT);;      
    gpio_set_level(DHT11_PIN, 0);;       
    ets_delay_us(19*1000); //19ms   

    //pull up and wait for senor response (20-40 us)
    gpio_set_level(DHT11_PIN, 1);;
    ets_delay_us(30);
    gpio_set_direction(DHT11_PIN, GPIO_MODE_INPUT);;

    //dht first sends out response signal then pulls up voltage before starting data transmission
    while(!gpio_get_level(DHT11_PIN));
    while(gpio_get_level(DHT11_PIN));
}

/*
Read one byte/8 bit of DHT11 data transmission. Starts with 50 us of low voltage to signal new data then a high voltage for data.
High voltage length of 26-28 us means "0", high voltage length greater than that is 1.
*/
uint8_t readData(void)
{ 
    uint8_t i,sbuf=0;
    for(i=0;i<8;i++)
    {
        //shift left by 1 to append new data transmission to least significant bit, eg. 00000001 becomes 00000010
        sbuf<<=1;
        //data transmission starts with low voltage level as signal so we skip this, then we add 30 us delay so that if data is 0 (26-28 us) then voltage after 30 us will pull down to 0
        while(!gpio_get_level(DHT11_PIN));
        ets_delay_us(30);
        //if high voltage after 30us, data was 1. Bitwise OR done so 1 bit will be added to the least significant bit eg. 00000010 becomes 00000011
        if(gpio_get_level(DHT11_PIN))
        {
            sbuf|=1;  
        }
        //if voltage after 30 us is 0, then data was 0. Bitwise OR will be done basically making the least significant bit 0 eg. 00000010 becomes 00000010
        else
        {
            sbuf|=0;
        }
        //
        while(gpio_get_level(DHT11_PIN));
    }
    return sbuf;   
}
/*Use readvalue function to get the 5 bytes needed*/
void getData(struct data *temp)
{
    uint8_t buf[5]={0};

    buf[0]=readData();
    buf[1]=readData();
    buf[2]=readData();
    buf[3]=readData();
    buf[4] =readData();

    //If the data transmission is right, the check-sum should be the last 8bit of "8bit integral RH data + 8bit decimal RH data + 8bit integral T data + 8bit decimal T data".
    if(buf[4] == buf[0]+buf[1]+buf[2]+buf[3])
        temp->status=0; //no error
    else
        temp->status=1; //error
    temp->temperature = buf[2];
    temp->humidity = buf[0];
} 
/*DHT11 section END*/


/*HTTP Server section START*/

/* Our URI handler function to be called during GET /uri request */
esp_err_t get_handler(httpd_req_t *req)
{
    /*gpio for temp*/
    gpio_pad_select_gpio(DHT11_PIN);
    startSignal();
    struct data currentData;
    getData(&currentData);
    if(currentData.status==0)
    {    
        printf("Temp=%d, Humi=%d\r\n",currentData.temperature,currentData.humidity);
    }
    else
    {
        printf("DHT11 Error!\r\n");
    }
    //char htmlPage[]="URI GET Response";
    char htmlPage[BUFFERSIZE]={0};
    sprintf(htmlPage,"<<!DOCTYPE html><html>\n<head>\n<style>\nhtml {font-family: sans-serif; text-align: center;}\n</style>\n</head>\n<body>\n<div>\n<h1>ESP32 IoT Server</h1>\n</div>\n<div>\n<h3>Temperature and Humidity Monitor</h3>\n<p>DHT11 Temperature Reading: %d&deg;C</p>\n<p>DHT11 Humidity Reading: %d%%</p>\n</div>\n</body>\n</html> >",currentData.temperature,currentData.humidity);
    httpd_resp_send(req, htmlPage, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* URI handler structure for GET /uri */
httpd_uri_t uri_get = {
    .uri      = "/",
    .method   = HTTP_GET,
    .handler  = get_handler,
    .user_ctx = NULL
};



/* Function for starting the webserver */
httpd_handle_t start_webserver()
{
    /* Generate default configuration */
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    /* Empty handle to esp_http_server */
    httpd_handle_t server = NULL;

    /* Start the httpd server */
    if (httpd_start(&server, &config) == ESP_OK) {
        /* Register URI handlers */
        httpd_register_uri_handler(server, &uri_get);
        //httpd_register_uri_handler(server, &uri_post);
    }
    /* If server failed to start, handle will be NULL */
    return server;
}



/*HTTP Server section END*/

void app_main(void)
{
    /*connect to wifi*/
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();

    /*start http server*/
    start_webserver();

}
