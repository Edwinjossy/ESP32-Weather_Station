#include "Cloud.h"
#include "string.h"

static const char *TAG = "CLOUD";
#define DELIM " ,;.:\"'-?!"

/* function prototype */
typedef void (*event_cb_t)(void *data, char *requestID);

/* event data management*/
typedef struct{
    char *eventName;
    event_cb_t eventFun;
    char *requestID;
}event_cb;

int eventCount=0;
event_cb *eventList;

/*
 * Function:  eventRegister 
 * --------------------
 *  Allows to register a callback function for a specific event name
 * 
 * event_cb_t fun: Callback function pointer
 * event_name: Event name to register
 */
void eventRegister(event_cb_t fun, char *event_name)
{
    if(eventCount==0)
        eventList=malloc(sizeof(event_cb));
    else
        eventList=realloc(eventList,sizeof(event_cb) * (eventCount+1));
    eventList[eventCount].eventFun=fun;
    eventList[eventCount].eventName=event_name;
    eventCount++;
}

/*
 * Function:  setHeater 
 * --------------------
 *  Callback that set SHT31 internal heater on/off
 * 
 * void *data: Event data
 */
static void setHeater(void *data,char *requestID )
{
    int value;
    sscanf((char *)data, "%d", &value);
    sht31_heater(value);
    stationdata.heater=value;
    char body[15];
    snprintf(body,sizeof(body)," {'heater':%d\}\ ",value);
    esp_mqtt_client_publish(mqtt_client, "v1/devices/me/telemetry", body, 0, 1, 0); 
}

/*
 * Function:  eventsHandler 
 * --------------------
 *  Call the appropriate Callback function when an event is raised
 * 
 * char *event_name: Event name
 * void *data: Data attribute of the event
 */
void eventsHandler(char *event_name, void *data, char *requestID)
{
    for(int i=0;i < eventCount; i++)
    {
        if(!strcmp(eventList[i].eventName,event_name))
        {
            printf("%s\n",requestID);
            eventList[i].requestID=requestID;
            eventList[i].eventFun(data,requestID);
            break;
        }
    }
}

/*
 * Function:  _http_event_handle 
 * --------------------
 *  Default handler for all the external http requests made from the ESP32
 */
esp_err_t _http_event_handle(esp_http_client_event_t *evt)
{ 
  switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI(TAG,"ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            break;
        case HTTP_EVENT_HEADER_SENT:
            break;
        case HTTP_EVENT_ON_HEADER:
            //ESP_LOGI(TAG,"%.*s", evt->data_len, (char*)evt->data);
            break;
        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(evt->client)) {
                ESP_LOGI(TAG,"%.*s", evt->data_len, (char*)evt->data);
            }
            //xEventGroupSetBits(xFlagsEventGroup,CLOUD_CONNECTED_BIT);
            break;
        case HTTP_EVENT_ON_FINISH:
            break;
        case HTTP_EVENT_DISCONNECTED:
            break;
    }
    return ESP_OK;
}

/*
 * Function:  update_wunderground 
 * --------------------
 *  Upload sensor data to WUnderground website
 * 
 * wuparameters_t *params - sensor data
 */
int update_wunderground(wuparameters_t *params)
{
    esp_err_t err;
    int lenght=0;
    char base_url[512];
    lenght+=snprintf(base_url,sizeof(base_url),"http://weatherstation.wunderground.com/weatherstation/updateweatherstation.php?ID=%s&PASSWORD=%s&dateutc=now&action=updateraw",WUNDERGORUND_STATIONID,WUNDERGORUND_STATIONPASS);
    if (params->temperature != -1 && params->humidity != -1)
        lenght+=snprintf(base_url+lenght,sizeof(base_url),"&tempf=%.2f&humidity=%.2f",params->temperature,params->humidity);
    if (params->wind_direction != -1)
        lenght+=snprintf(base_url+lenght,sizeof(base_url),"&winddir=%d",params->wind_direction);
    if (params->wind_speedmph != -1)
        lenght+=snprintf(base_url+lenght,sizeof(base_url),"&windspeedmph=%.3f",params->wind_speedmph);
    if (params->dailyrain != -1)
        lenght+=snprintf(base_url+lenght,sizeof(base_url),"&dailyrainin=%.4f",params->dailyrain);
    if (params->devpoint != -1)
        lenght+=snprintf(base_url+lenght,sizeof(base_url),"&dewptf=%.4f",params->devpoint);
    if (params->wind_gustdirection != -1)
        lenght+=snprintf(base_url+lenght,sizeof(base_url),"&windgustdir=%d",params->wind_gustdirection);
    if (params->wind_gustmph != -1)
        lenght+=snprintf(base_url+lenght,sizeof(base_url),"&windgustmph=%.3f",params->wind_gustmph);


    esp_http_client_config_t config = {
    .url = base_url,
    .event_handler = _http_event_handle,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_method(client, HTTP_METHOD_GET);
    err = esp_http_client_perform(client);
    esp_http_client_cleanup(client);
    return 0;
}


/*
 * Function:  reset_parameters 
 * --------------------
 *  Initialize the sensor data struct to default values
 * 
 * wuparameters_t *params - struct to inizialize
 */
void reset_parameters(wuparameters_t *params)
{
    params->dailyrain=params->wind_gustmph=params->wind_gustdirection=params->humidity=params->temperature=params->wind_direction=params->wind_speedmph=params->devpoint=-1;
    params->heater=0;
}

/*
 * Function:  mqtt_event_handler 
 * --------------------
 *  Default handler for all the external mqtt requests
 */
static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    mqtt_client = event->client;
    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_BEFORE_CONNECT:
            break;           
        case MQTT_EVENT_ANY:
            break;
        case MQTT_EVENT_CONNECTED:
            break;
        case MQTT_EVENT_DISCONNECTED:
            break;
        case MQTT_EVENT_SUBSCRIBED:
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            break;
        case MQTT_EVENT_PUBLISHED:
            break;
        case MQTT_EVENT_DATA:
            //ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            /* json to c */
            char *rest=event->data;
            char *token;
            int count=0;
            char *event_name=NULL;
            char *event_data=NULL;
            while ((token = strtok_r(rest, " ,;.:\"'-?!", &rest)))
            {
                count++;
                if(count==3)
                    event_name=token;
                if(count==5)
                    event_data=token;               
            }
            eventsHandler(event_name,event_data,(event->topic)+26);
            break;
        case MQTT_EVENT_ERROR:
            break;
    }
    return ESP_OK;
}

/*
 * Function:  serializeJSON 
 * --------------------
 *  Allow to serialize a struct wuparameters_t to a json string, suitable for uploading to thingsboard
 * 
 * wuparameters_t parameter - data to serialize to json
 */
char *serializeJSON(wuparameters_t parameter) 
{
    /* calculate string dimension */ 
    size_t str_len=0;
    str_len= snprintf(NULL,str_len,"{'temperature':%.2f,'humidity':%.2f,'winddir':%d,'windspeed':%.2f,'dailyrain':%.2f,'dewptf':%.4f,'heater':%d\}\ ",parameter.temperature,
    parameter.humidity,parameter.wind_direction,parameter.wind_speedmph,parameter.dailyrain,parameter.devpoint,parameter.heater);

    char *final_str= calloc(1,sizeof(char) * str_len + 1);
    snprintf(final_str,str_len,"{'temperature':%.2f,'humidity':%.2f,'winddir':%d,'windspeed':%.2f,'dailyrain':%.2f,'dewptf':%.4f,'heater':%d\}\ ",parameter.temperature,
    parameter.humidity,parameter.wind_direction,parameter.wind_speedmph,parameter.dailyrain,parameter.devpoint,parameter.heater);
    return final_str;    
}

/*
 * Function:  mqtt_app_start 
 * --------------------
 *  Initialize MQTT manager
 */
void mqtt_app_start()
{
    /* Thingsboard configuration struct */ 
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = THINGSBOARDURL,
        .event_handle = mqtt_event_handler,
        .port = THINGSBOARDPORT,
        .username = THINGSBOARDTOKEN
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(mqtt_client);

    /* register callback functions*/
    eventRegister(setHeater,"setHeater");

    vTaskDelay(1000 / portTICK_PERIOD_MS);
}