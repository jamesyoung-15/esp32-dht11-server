# ESP32 DHT11 IoT Project
This is a simple IoT project that displays the current temperature and humidity on a webpage. The program converts the signal received through the ESP32 GPIO from the DHT11 and converts it to integers that represent the temperature and humidity. Then the data is written into an HTML file so that it can be displayed on the web via an HTTP server on the ESP32. I used ESP-IDF framework for this project.

## Demo
![alt text](https://github.com/jamesyoung-15/esp32-dht11-server/tree/main/images/esp-dht11-demo.gif)

## Usage
Install ESP-IDF framework and it up according to their documentation.
``` bash
git clone https://github.com/jamesyoung-15/esp32-dht11-server.git
cd esp32-dht11-server
idf.py fullclean
idf.py build
idf.py flash -p /dev/ttyUSB0 #Or whichever location ESP32 is in, also check permissions
```
Server takes a few seconds to work. Since this server serves a static page, the temperature and humidity are not dynamically updated and client has to refresh webpage to see latest temperature.

## Reading from DHT11
Datasheet: https://www.mouser.com/datasheet/2/758/DHT11-Technical-Data-Sheet-Translated-Version-1143054.pdf

Using the datasheet above, we can obtain the tempertaure and humidity readings from the ESP32 GPIO. Below is the basic outline of how it works.

Basic structure:
1. MCU sends out start signal to dht11 by pulling down voltage for at least 18 ms
2. MCU pulls up voltage and waits for dht11 to respond (20-40ms)
3. DHT11 sends out low response signal for 80 us, then pulls up for 80us and readies for data transmission
4. Data transmission is sent, total of 40 bits or 5 bytes. Data format: [int rh, float rh, int temp, float temp, checksum]

## Connecting to Wifi
Mainly taken from the ESP-IDF wifi-station example [here](https://github.com/espressif/esp-idf/tree/master/examples/wifi/getting_started/station). Didn't change much other than setting up variables.

## Setting up HTTP Server
Mainly referred to the documentation [here](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_http_server.html) for implementing the server. For the webpage I stored the static html page in a char array then we use sprintf to write the obtained tempertature and humidity data to the html page. While this doesn't allow for a "live" realtime monitoring, it shows the temperature at the given time the webpage is requested.