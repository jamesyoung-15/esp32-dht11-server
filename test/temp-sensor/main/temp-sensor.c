/* 
Main reference: https://www.mouser.com/datasheet/2/758/DHT11-Technical-Data-Sheet-Translated-Version-1143054.pdf
Basic structure:
1. MCU sends out start signal to dht11 by pulling down voltage for at least 18 ms
2. MCU pulls up voltage and waits for dht11 to respond (20-40ms)
3. DHT11 sends out low response signal for 80 us, then pulls up for 80us and readies for data transmission
4. Data transmission is sent, total of 40 bits or 5 bytes. Data format: [int rh, float rh, int temp, float temp, checksum]

*/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

#define DHT11_PIN     4

struct data{
    int temperature,humidity,status;
};

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

void app_main(void)
{
    gpio_pad_select_gpio(DHT11_PIN);
    while(1) {
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
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
