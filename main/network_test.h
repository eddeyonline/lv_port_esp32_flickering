#ifndef NETWORK_TEST_H
#define NETWORK_TEST_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "esp_freertos_hooks.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"

#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"

#define MODE_AP_STA 1

#define EXAMPLE_ESP_MAXIMUM_RETRY   5

void event_handler(void* arg, esp_event_base_t event_base,
                        int32_t event_id, void* event_data);

void wifi_init_sta(void);

#endif
