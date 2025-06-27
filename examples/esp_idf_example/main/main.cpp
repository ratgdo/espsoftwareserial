#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "SoftwareSerial.h"

extern "C" void app_main(void)
{
    printf("ESP-IDF SoftwareSerial Example\n");
    
    // Create software serial instance on pins 16 (RX) and 17 (TX)
    EspSoftwareSerial::UART swSerial;
    swSerial.begin(9600, EspSoftwareSerial::SWSERIAL_8N1, 16, 17);
    
    // Send test message
    swSerial.println("Hello from ESP-IDF!");
    
    // Echo loop
    while (true) {
        if (swSerial.available()) {
            char c = swSerial.read();
            printf("Received: %c\n", c);
            swSerial.write(c); // Echo back
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}