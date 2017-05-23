#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include "driver/gpio.h"
#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"

#include "sixxlowpan_test.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

//Just a test main file, created by the BTStack task

void user_task(void)
{
	struct hostent *hp;
	int level = 0;
	uint8_t test = 0;
    //esp_err_t ret;
    
    gpio_set_direction(GPIO_NUM_5, GPIO_MODE_OUTPUT);
    
	while(1)
	{
		for(int i = 0; i<100; i++)
		{
			vTaskDelay(10 / portTICK_PERIOD_MS);
			//esp_task_wdt_feed();
		}
		
        gpio_set_level(GPIO_NUM_5, level);
        level = !level;	
        
        if((xEventGroupGetBits(lowpan_ble_flags) & LOWPAN_BLE_FLAG_CONNECTED))
        {   
		    printf("Get hostname....\n");
		    hp = gethostbyname("www.google.at");
		    if(hp != NULL)
		    {
				printf("Result: addr type: %d, name: %s\n",hp->h_addrtype,hp->h_name);
			} else {
				printf("No result...\n");
			}

		    test = 1;
		}
	}
}
