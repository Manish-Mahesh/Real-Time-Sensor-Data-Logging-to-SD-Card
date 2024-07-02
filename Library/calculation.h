#ifndef CALCULATION_H
#define CALCULATION_H

#include "mbed.h"
extern I2C i2c;
extern uint16_t dig_T1;
extern int16_t dig_T2, dig_T3;
extern uint16_t dig_P1;
extern int16_t dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
extern float temp_fine;

void read_calibration_data();
void read_raw_data(int32_t &raw_temp, int32_t &raw_press);
int32_t calc_temp(int32_t raw_temp);
uint32_t calc_press(int32_t raw_press);

#endif 