/*
* Interface for Temperature & Humidity sensor 
* Using SHT31 Module
*/
#ifndef MY_SHT31_H
#define MY_SHT31_H

#define SDA_PIN 21  
#define SCL_PIN 22
#define ACK_VAL 0x0
#define NACK_VAL 0x1


typedef struct {
    double temperature;
    double humidity;
}shtvalues_t;


void sht31_setup();
shtvalues_t sht31_getdata(); 

void sht31_heater(int status);
#endif