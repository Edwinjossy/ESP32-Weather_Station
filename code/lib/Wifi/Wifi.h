/*
* Interface for wind speed and wind direction
* Using Sparkfun Wind module
*/
#ifndef MY_WIFI_H
#define MY_WIFI_H
#include <freertos/FreeRTOS.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/event_groups.h"
#include "esp_log.h"

#define WIFI_SSID "SSID"
#define WIFI_PASS "WIFIPASS"

static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data);
void wifi_init_sta( );
#endif