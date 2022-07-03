#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_log.h"
#include "driver/i2c.h"
#include <sht31.h>


i2c_config_t conf;

/*
 * Function:  sht31_setup 
 * --------------------
 * Initialize i2c comunications
 */
void sht31_setup()
{
    /* configure the wather station as node Master */
    conf.mode = I2C_MODE_MASTER;
	conf.sda_io_num = SDA_PIN;
	conf.scl_io_num = SCL_PIN;
    /* Pullup resistors on PCB, onboard ones disabled */
	conf.sda_pullup_en = GPIO_PULLUP_DISABLE;
	conf.scl_pullup_en = GPIO_PULLUP_DISABLE;
	conf.master.clk_speed = 100000;    
    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0));
}

/*
 * Function:  sht31_getdata 
 * --------------------
 * Get temperature and humdity values from the SHT31 sensor
 * 
 * return: shtvalues_t<double,double> containing temperature(F°) and humidty values 
 */
shtvalues_t sht31_getdata()
{
    shtvalues_t sensor_values;
	/* Initialize comunication with the sensor*/ 
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_write_byte(cmd, (0x44 << 1) | I2C_MASTER_WRITE, 1));
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_write_byte(cmd, 0x2C, true));
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_write_byte(cmd, 0x06, true));
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_stop(cmd));
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS));
    vTaskDelay(500 / portTICK_PERIOD_MS); 
    /* Reading temperature and humidity */
    uint8_t temp_msb, temp_lsb,hc_msb, hc_lsb,temp_crc, hc_crc;
    cmd = i2c_cmd_link_create();
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_start(cmd));
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_write_byte(cmd, (0x44 << 1) | I2C_MASTER_READ, true));
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_read_byte(cmd, &temp_msb, 0));
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_read_byte(cmd, &temp_lsb, 0));
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_read_byte(cmd, &temp_crc, 0));
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_read_byte(cmd, &hc_msb, 0));
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_read_byte(cmd, &hc_lsb, 0));
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_read_byte(cmd, &hc_crc, 1));
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_stop(cmd));
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_RATE_MS));

    /*TO-DO calculate and check CRC */

    double raw_tempD= ((temp_msb << 8)| temp_msb);
    double raw_hcD= ((hc_msb << 8)| hc_lsb);
    /* Formula to get Temperature in F° from raw data (SHT31 datasheet) */
    sensor_values.temperature= (raw_tempD * 315.0 / 65535.0) - 49.0;
    sensor_values.humidity=100.0* (raw_hcD / 65535.0 );
    i2c_cmd_link_delete(cmd); 
    return sensor_values;
}

/*
 * Function:  sht31_heater 
 * --------------------
 * Handle sht31 on board heater
 * 
 * status: 0 turn the heater off; 1 turn the heater on;
 */
void sht31_heater(int status)
{
    /* create command link */ 
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_start(cmd));
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_write_byte(cmd, (0x44 << 1) | I2C_MASTER_WRITE, 1));

    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_write_byte(cmd, 0x30, true));
    if(status)
        ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_write_byte(cmd, 0x6D, true));
    else
        ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_write_byte(cmd, 0x66, true));
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_stop(cmd));
    ESP_ERROR_CHECK_WITHOUT_ABORT(i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS));

    i2c_cmd_link_delete(cmd); 
}