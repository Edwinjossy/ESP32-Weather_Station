/*
* Interface for wind speed and wind direction
* Using Sparkfun Wind module
*/
#ifndef MY_CLOUD_H
#define MY_CLOUD_H
#include <freertos/FreeRTOS.h>
#include "esp_http_client.h"
#include "freertos/event_groups.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "sht31.h"


#define WUNDERGORUND_STATIONID "WUNDERGROUNDID"
#define WUNDERGORUND_STATIONPASS "WUNDERGROUNDPASS"
#define THINGSBOARDTOKEN "THINGSBOARDTOKEN"
#define THINGSBOARDURL "mqtt://THINGSBOARDURL"
#define THINGSBOARDPORT 1883
#define CLOUD_CONNECTED_BIT BIT1

static EventGroupHandle_t xFlagsEventGroup;
esp_mqtt_client_handle_t mqtt_client;

typedef struct {
    double temperature;
    double humidity;
    int wind_direction;
    int wind_gustdirection;
    int heater;
    float wind_speedmph;
    float wind_gustmph;
    float dailyrain;
    double devpoint;
}wuparameters_t;

/* defined here but initialized in main.c */ 
extern wuparameters_t stationdata;

int update_wunderground(wuparameters_t *params);
void reset_parameters(wuparameters_t *params);
void mqtt_app_start();
char *serializeJSON(wuparameters_t parameter); 

#endif