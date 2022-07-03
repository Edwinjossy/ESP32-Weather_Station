#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <math.h>
#include <sdkconfig.h>
#include <driver/adc.h>
#include "esp_log.h"
#include "string.h"
#include "sht31.h"
#include "Wifi.h"
#include "Cloud.h"
#include "nvs_flash.h"
#include "esp_sntp.h"

#ifndef UDP_LOGGING_MAX_PAYLOAD_LEN
#define UDP_LOGGING_MAX_PAYLOAD_LEN 2048
#endif

#define STATUS_LED 27
#define CLOUD_LED 26
#define RAIN_PIN 23
#define WINDSPEED_PIN 35
#define ESP_INTR_FLAG_DEFAULT 0
#define STATUS_BIT BIT0
#define BIT_1	( 0 << 0 )

time_t now;
struct tm timeinfo;
int rain_clicks,wind_clicks;
int current_day,last_day;
float rain_roday;
shtvalues_t Last_sht31values;
wuparameters_t stationdata;
TickType_t xLastRainTime,xRainTime,xLastWindTime,xWindTime,XLastWindCheck;
static uint8_t buffer[UDP_LOGGING_MAX_PAYLOAD_LEN];


/*
 * Function:  blink_task 
 * --------------------
 * 
 *    
 *
 * 
 */
void blink_task(void *pvParameter)
{ 
    TickType_t xLastWakeTime;
    xLastWakeTime= xTaskGetTickCount();
    const TickType_t xFrequency = 1000 / portTICK_PERIOD_MS; //60s
    gpio_reset_pin(STATUS_LED);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(STATUS_LED, GPIO_MODE_OUTPUT);

    while(1)
    {
        EventBits_t bits = xEventGroupGetBits(xFlagsEventGroup);
        if (bits & STATUS_BIT) {
            gpio_set_level(STATUS_LED, 1);
            vTaskDelayUntil(&xLastWakeTime,xFrequency);
            
        }
        else
        {
            gpio_set_level(STATUS_LED, 0);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            xEventGroupSetBits(xFlagsEventGroup,STATUS_BIT );
        }
    }
}

/*
 * Function:  cloudupdate_task 
 * --------------------
 * 
 *    
 *
 * 
 */
void cloudupdate_task(void *pvParameter)
{ 
    TickType_t xLastWakeTime;
    xLastWakeTime= xTaskGetTickCount();
    const TickType_t xFrequency = 5000 / portTICK_PERIOD_MS; 
    while(1)
    {
        vTaskDelayUntil(&xLastWakeTime,xFrequency);
        update_wunderground(&stationdata);
        char *json=serializeJSON(stationdata);
        esp_mqtt_client_publish(mqtt_client, "v1/devices/me/telemetry", json, 0, 1, 0);      
    }
    vTaskDelete(NULL);
}

// reference: http://en.wikipedia.org/wiki/Dew_point
double dewPointFast(double celsius, double humidity)
{
        double a = 17.271;
        double b = 237.7;
        double temp = (a * celsius) / (b + celsius) + log(humidity*0.01);
        double Td = (b * temp) / (a - temp);
        return Td;
}

/*
 * Function:  temperature_task 
 * --------------------
 * 
 */
void temperature_task(void *pvParameter)
{
    TickType_t xLastWakeTime;
    xLastWakeTime= xTaskGetTickCount();
    const TickType_t xFrequency_sensing = 600000 / portTICK_PERIOD_MS; /* 10 minutes */
    const TickType_t xDelay = 25000 / portTICK_PERIOD_MS; /* 25 seconds */
    const TickType_t xDelayHeater = 180000 / portTICK_PERIOD_MS; /* 3 minutes */
    sht31_setup();
    Last_sht31values = sht31_getdata();

    while (1)
    {
        shtvalues_t temp_values = sht31_getdata();
        /* faulty reading */ 
        if(fabs(temp_values.temperature - Last_sht31values.temperature) > 10 ||  temp_values.temperature == -1 || temp_values.humidity== -1) 
        {
            /* clear status bit to signal error */
            xEventGroupClearBits(xFlagsEventGroup,STATUS_BIT );
            ESP_LOGI("SHT31","Error sensing temperature, error greater than 10°");
            vTaskDelayUntil(&xLastWakeTime,xDelay);
            continue;
        }
          
        if( temp_values.humidity >= 90)
        {
            /* turn on heater for 2 minutes, cool off 20 secs and take another measurement */
            sht31_heater(1);
            stationdata.heater=1;
            vTaskDelayUntil(&xLastWakeTime,xDelayHeater);
            sht31_heater(0);
            stationdata.heater=0;
            vTaskDelayUntil(&xLastWakeTime,xDelay);
            temp_values = sht31_getdata();
            /* Correct values drift due to heater being on */
            temp_values.humidity= (Last_sht31values.humidity + temp_values.humidity) / 2;
            //temp_values.temperature-=3.0; 
        }

        /* update sensor values */ 
        Last_sht31values=temp_values;   

        stationdata.temperature=temp_values.temperature; 
        stationdata.humidity=temp_values.humidity;     
        double tempc = (stationdata.temperature - 32) * 5/9;
        stationdata.devpoint = (dewPointFast(tempc,stationdata.humidity) * 9 / 5) + 32; // convert dewpoint to F°
        vTaskDelayUntil(&xLastWakeTime,xFrequency_sensing);
        
    }
}

void winddir_task(void *pvParameter)
{  
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_11db);
    TickType_t xLastWakeTime;
    xLastWakeTime= xTaskGetTickCount();
    const TickType_t xFrequency = 2000 / portTICK_PERIOD_MS;
    while(1){    
        int read=adc1_get_raw(ADC1_CHANNEL_6);
        int dir; 
        if (read >=3684) dir=270;  
        else if (read >=3287) dir=315;  
        else if (read >=3100) dir=300;  
        else if( read >=2812) dir=360;  
        else if (read >=2508) dir=340;  
        else if (read >=2350) dir=225;  
        else if (read >=2230) dir=250;
        else if (read >=1594) dir=45;  
        else if (read >=1377) dir=30;  
        else if (read >=931) dir=180;  
        else if (read >=800) dir=210;  
        else if (read >=547) dir=135;  
        else if (read >=345) dir=160;  
        else if (read >=209) dir=90;  
        else if (read >=178) dir=70;  
        else if (read >=107) dir=120;    
        else dir=-1;
        stationdata.wind_direction=dir;
        vTaskDelayUntil(&xLastWakeTime,xFrequency);
    } 
}


void IRAM_ATTR wind_IRQ_handler(void* arg) {
    xWindTime= xTaskGetTickCount();
    if(xWindTime - xLastWindTime > 15 / portTICK_PERIOD_MS)
    {
        wind_clicks++;
        xLastWindTime=xWindTime;
    }
}

/*
 * Function:  windspeed_task 
 * --------------------
 * 
 */
void windspeed_task(void *pvParameter)
{
    TickType_t xLastWakeTime;
    xLastWakeTime= xTaskGetTickCount();
    const TickType_t xFrequency = 1000 / portTICK_PERIOD_MS; 
    gpio_set_direction(WINDSPEED_PIN, GPIO_MODE_INPUT);
    // enable interrupt on falling (1->0) edge for button pin
	gpio_set_intr_type(WINDSPEED_PIN, GPIO_INTR_NEGEDGE);
    // install ISR service with default configuration
	gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);	
	// attach the interrupt service routine
	gpio_isr_handler_add(WINDSPEED_PIN, wind_IRQ_handler, NULL);
    while (1)
    {
        wind_clicks=0;
        vTaskDelayUntil(&xLastWakeTime,xFrequency);  
        float windSpeed = (float)wind_clicks; // 2; 
        windSpeed *= 1.492;
        stationdata.wind_speedmph=windSpeed;
        if(windSpeed > stationdata.wind_gustmph)
        {
            stationdata.wind_gustdirection=stationdata.wind_direction;
            stationdata.wind_gustmph=windSpeed;
        }
        printf("Counter %d speed:%.3f gust:%.3f\n",wind_clicks,windSpeed,stationdata.wind_gustmph);         
    }
}


void IRAM_ATTR rain_IRQ_handler(void* arg) {
    xRainTime= xTaskGetTickCount();
    if(xRainTime - xLastRainTime > 125 / portTICK_PERIOD_MS)
    {
        rain_clicks++;
        stationdata.dailyrain += 0.011; //0.011" of water   
        xLastRainTime=xRainTime;
    }
}

/*
 * Function:  raingauge_task 
 * --------------------
 * 
 *    
 *
 * 
 */
void raingauge_task(void *pvParameter)
{
    TickType_t xLastWakeTime;
    xLastWakeTime= xTaskGetTickCount();
    const TickType_t xFrequency = 1000 / portTICK_PERIOD_MS; //5s

    //gpio_pad_select_gpio(15);
    gpio_set_direction(RAIN_PIN, GPIO_MODE_INPUT);
    // enable interrupt on falling (1->0) edge for button pin
	gpio_set_intr_type(RAIN_PIN, GPIO_INTR_NEGEDGE);
    // install ISR service with default configuration
	gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);	
	// attach the interrupt service routine
	gpio_isr_handler_add(RAIN_PIN, rain_IRQ_handler, NULL);
    stationdata.dailyrain=0;
    while (1)
    {
        /* check if a day has passed */
        if(current_day != last_day)
        {
            stationdata.dailyrain=0;
            last_day=current_day;
        }
        rain_clicks=0;
        vTaskDelayUntil(&xLastWakeTime,xFrequency);  
        printf("Counter:%d daily:%.4f\n",rain_clicks,stationdata.dailyrain);  
    }
}

/*
 * Function:  initialize_sntp 
 * --------------------
 *  
 */
static void initialize_sntp(void)
{
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
}

int getdayM()
{
    localtime_r(&now, &timeinfo);
    /*wait to sincronize time */
	while(timeinfo.tm_year < (2021 - 1900)) {		
		vTaskDelay(1500 / portTICK_PERIOD_MS);
		time(&now);
        localtime_r(&now, &timeinfo);
	}
    return timeinfo.tm_mday;
}

void updateday_task(void *pvParameter)
{
    TickType_t xLastWakeTime;
    xLastWakeTime= xTaskGetTickCount();
    const TickType_t xFrequency = 60000 / portTICK_PERIOD_MS; //10 minutes
    while (1)
    {
        vTaskDelayUntil(&xLastWakeTime,xFrequency);
        current_day=getdayM();
    }   
}

int udp_logging_vprintf( const char *str, va_list l ) {
    int len;
	len = vsprintf((char*)buffer, str, l);
    /* calculate string dimension */ 
    size_t str_len=0;
    str_len= snprintf(NULL,str_len,"{'log':'%s'\}\ ",buffer);

    char *final_str= calloc(1,sizeof(char) * str_len + 1);
    snprintf(final_str,str_len,"{'log':'%s'\}\ ",buffer);


    esp_mqtt_client_publish(mqtt_client, "v1/devices/me/telemetry", final_str, 0, 1, 0);

	return vprintf( str, l );
}

void app_main() {
    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* create the flags event group. */
	xFlagsEventGroup = xEventGroupCreate();
    EventBits_t uxBits;

    /* Set bit 0 */
    uxBits = xEventGroupSetBits(xFlagsEventGroup,STATUS_BIT );
   
    /* Initialize Wifi */   
    wifi_init_sta();

    /* Initialize SNTP */
    initialize_sntp();	
	time(&now);
    /* Set italian timezone */   
	setenv("TZ", "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00", 1);
	tzset();
    /* set current day */
    current_day=last_day=getdayM();

    reset_parameters(&stationdata);
 
	fflush( stdout );

    /* Initialize MQTT */
    mqtt_app_start();
    /* subscribe to RCP commands */
    esp_mqtt_client_subscribe(mqtt_client,"v1/devices/me/rpc/request/+",1);  

    /* change logging system */ 
    esp_log_set_vprintf(udp_logging_vprintf);

    /* TASKS */
    xTaskCreate(&updateday_task,"update current day",4000,NULL,4,NULL);
    xTaskCreate(&blink_task,"blink_task",4000,NULL,4,NULL);
    xTaskCreate(&temperature_task,"temperature check",4000,NULL,4,NULL);
    xTaskCreate(&winddir_task,"wind direction check",4000,NULL,4,NULL);
    xTaskCreate(&windspeed_task,"wind speed check",4000,NULL,4,NULL);
    xTaskCreate(&raingauge_task,"rain gauge check",4000,NULL,4,NULL);
    xTaskCreate(&cloudupdate_task,"cloud update task",4000,NULL,5,NULL);   
}