
#include "calculation.h"
#include "mbed.h"
// I2C address and calibration data
#define I2C_ADDR 0x76 << 1
#define CALIB00 0x88
#define REG_CTRL_MEAS 0xF4
#define REG_PRESS_MSB 0xF7
#define I2C_SDA PB_9
#define I2C_SCL PB_8

uint16_t dig_T1;
int16_t dig_T2, dig_T3;
uint16_t dig_P1;
int16_t dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
float temp_fine;
I2C i2c(I2C_SDA, I2C_SCL);
void read_calibration_data() {
    char data[24];
    char reg = CALIB00;
    i2c.write(I2C_ADDR, &reg, 1, true);
    i2c.read(I2C_ADDR, data, 24);

    dig_T1 = (data[1] << 8) | data[0];
    dig_T2 = (data[3] << 8) | data[2];
    dig_T3 = (data[5] << 8) | data[4];
    dig_P1 = (data[7] << 8) | data[6];
    dig_P2 = (data[9] << 8) | data[8];
    dig_P3 = (data[11] << 8) | data[10];
    dig_P4 = (data[13] << 8) | data[12];
    dig_P5 = (data[15] << 8) | data[14];
    dig_P6 = (data[17] << 8) | data[16];
    dig_P7 = (data[19] << 8) | data[18];
    dig_P8 = (data[21] << 8) | data[20];
    dig_P9 = (data[23] << 8) | data[22];
}

void read_raw_data(int32_t &raw_temp, int32_t &raw_press) {
    char data[6];
    char cmd[2] = {REG_CTRL_MEAS, 0x27};
    i2c.write(I2C_ADDR, cmd, 2);
    ThisThread::sleep_for(100ms);
    char reg = REG_PRESS_MSB;
    i2c.write(I2C_ADDR, &reg, 1, true);
    i2c.read(I2C_ADDR, data, 6);

    raw_press = (data[0] << 12) | (data[1] << 4) | (data[2] >> 4);
    raw_temp = (data[3] << 12) | (data[4] << 4) | (data[5] >> 4);
}

int32_t calc_temp(int32_t raw_temp) {
    int32_t temp;
#if (BMP280_CALC_TYPE_INTEGER)
    // Integer calculations
    temp_fine = ((((raw_temp >> 3) - ((int32_t)dig_T1 << 1))) * ((int32_t)dig_T2)) >> 11;
    temp_fine += (((((raw_temp >> 4) - ((int32_t)dig_T1)) * ((raw_temp >> 4) - ((int32_t)dig_T1))) >> 12) * ((int32_t)dig_T3)) >> 14;
    temp = ((temp_fine * 5) + 128) >> 8;
    return temp;
#else
    // Float calculations
    float var1, var2;

    var1 = (((float)raw_temp) / 16384.0F - ((float)dig_T1) / 1024.0F) * ((float)dig_T2);
    var2 = ((float)raw_temp) / 131072.0F - ((float)dig_T1) / 8192.0F;
    var2 = (var2 * var2) * ((float)dig_T3);
    temp_fine = var1 + var2;
    temp = (int32_t)((temp_fine / 5120.0F) * 100.0F);
    return temp;
#endif
}

uint32_t calc_press(int32_t raw_press) {
    uint32_t p;
#if (BMP280_CALC_TYPE_INTEGER)
    // 32-bit only calculations
    int32_t var1, var2;

    var1 = (((int32_t)temp_fine) >> 1) - (int32_t)64000;
    var2 = (((var1 >> 2) * (var1 >> 2)) >> 11) * ((int32_t)dig_P6);
    var2 = var2 + ((var1 * ((int32_t)dig_P5)) << 1);
    var2 = (var2 >> 2) + (((int32_t)dig_P4) << 16);
    var1 = (((dig_P3 * (((var1 >> 2) * (var1 >> 2)) >> 13)) >> 3) + ((((int32_t)dig_P2) * var1) >> 1)) >> 18;
    var1 = (((32768 + var1)) * ((int32_t)dig_P1)) >> 15;
    if (var1 == 0)
    {
        // avoid exception caused by division by zero
        return 0;
    }
    p = (((uint32_t)(((int32_t)1048576) - raw_press) - (uint32_t)(var2 >> 12))) * 3125U;
    if (p < 0x80000000U)
    {
        p = (p << 1) / ((uint32_t)var1);
    }
    else
    {
        p = (p / (uint32_t)var1) << 1;
    }
    var1 = (((int32_t)dig_P9) * ((int32_t)(((p >> 3) * (p >> 3)) >> 13))) >> 12;
    var2 = (((int32_t)(p >> 2)) * ((int32_t)dig_P8)) >> 13;
    p = (uint32_t)((int32_t)p + ((var1 + var2 + dig_P7) >> 4));
    // p = p * 1000U; // convert to mPa
    return p;
#else
    // Float calculations
    float var1, var2, p_f;
    var1 = (temp_fine / 2.0F) - 64000.0F;
    var2 = var1 * var1 * ((float)dig_P6) / 32768.0F;
    var2 = var2 + var1 * ((float)dig_P5) * 2.0F;
    var2 = (var2 / 4.0F) + (((float)dig_P4) * 65536.0F);
    var1 = (((float)dig_P3) * var1 * var1 / 524288.0F + ((float)dig_P2) * var1) / 524288.0F;
    var1 = (1.0F + var1 / 32768.0F) * ((float)dig_P1);

    p_f = 1048576.0F - (float)raw_press;

    if ((uint32_t)var1 == 0U)
    {
        // Avoid exception caused by division by zero
        return 0;
    }
    p_f = (p_f - (var2 / 4096.0F)) * 6250.0F / var1;
    var1 = ((float)dig_P9) * p_f * p_f / 2147483648.0F;
    var2 = p_f * ((float)dig_P8) / 32768.0F;
    p_f += (var1 + var2 + ((float)dig_P7)) / 16.0F;
    // p = (uint32_t)(p_f * 1000.0F); //convert to mPa
    p = (uint32_t)(p_f);
    return p;
#endif
}