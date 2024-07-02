#include "mbed.h"
#include "SDBlockDevice.h"
#include "FATFileSystem.h"
#include "calculation.h"
#include <cstring>
#include <cstdio>
#include <ctime>
#include <cstdlib>
#include "rtos.h"

#define FORMAT_SD_CARD true

SDBlockDevice sd(MBED_CONF_SD_SPI_MOSI,
                 MBED_CONF_SD_SPI_MISO,
                 MBED_CONF_SD_SPI_CLK,
                 MBED_CONF_SD_SPI_CS);

FATFileSystem fs("sd", &sd);

AnalogIn capacitorVoltage(A0);
DigitalOut dischargePin(PB_7);
DigitalOut disableSolarPin(PB_5);
InterruptIn userButton(PB_2);
AnalogIn solarCurrent(A2);

const float Resistance = 8.43; 
const float REFERENCE_VOLTAGE = 3.3; 
const float MIN_VOLTAGE = 0.5; 
const float MAX_VOLTAGE = 2.7; 
volatile bool buttonPressed = false;

Mutex fileMutex;

void handleButtonPress() {
    buttonPressed = true;
}
time_t currentTime;

void set_time_via_serial_input() {
    printf("Enter date and time in format: YYYY-MM-DD hh:mm:ss\r\n");

    int day, month, year, hour, minute, second;

    int scan_result = scanf("%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second);

    if (scan_result < 6) {
        printf("Failed to read the serial input.\nPlease press reset and try again.\n");
    } else {
        struct tm t;
        t.tm_sec = second;
        t.tm_min = minute;
        t.tm_hour = hour;
        t.tm_mday = day;
        t.tm_mon = month - 1;
        t.tm_year = year - 1900;

        time_t user_time = mktime(&t); 

        set_time(user_time); 
        printf("Date and time set successfully! \n");
    }
}

void print_datetime_formatted() {
    time(&currentTime);
    struct tm *current_tm = localtime(&currentTime);
    
    printf("Current date and time: %04d-%02d-%02d %02d:%02d:%02d\r\n",
            current_tm->tm_year + 1900, current_tm->tm_mon + 1, current_tm->tm_mday,
            current_tm->tm_hour, current_tm->tm_min, current_tm->tm_sec);
}

float calculateSoC(float voltage) {
    float SoC = ((voltage * voltage) - (MIN_VOLTAGE * MIN_VOLTAGE)) / ((MAX_VOLTAGE * MAX_VOLTAGE) - (MIN_VOLTAGE * MIN_VOLTAGE)) * 100.0;
    return SoC;
}

void check_error(int err) {
    printf("%s\n", (err < 0 ? "Fail :(" : "OK"));
    if (err < 0) {
        error("error: %s (%d)\n", strerror(err), -err);
    }
}

void create_log_filename(char *filename, size_t len) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(filename, len, "/sd/%Y-%m-%d_%H-%M-%S.log", t);
}

void delete_all_log_files() {
    printf("Deleting all log files...\n");
    DIR *dir = opendir("/sd/");
    struct dirent *de;
    while ((de = readdir(dir)) != NULL) {
        char file_path[256];
        snprintf(file_path, sizeof(file_path), "/sd/%s", de->d_name);
        remove(file_path);
    }
    closedir(dir);
    printf("All log files deleted.\n");
}

// A structure to store the measurement data
struct MeasurementData {
    char timestamp[16];
    int pressure;
    float temperature;
    int solar_current;
    float cap_voltage;
    int charge_level;
};

int main() {
    userButton.fall(&handleButtonPress);
    set_time_via_serial_input();
    int err = 0;
    int i = 0;
    printf("Welcome to the filesystem example.\n");

    printf("Mounting the filesystem... \n");
    err = fs.mount(&sd);    

    if (FORMAT_SD_CARD) {
        printf("Formatting the SD Card... ");
        fflush(stdout);
        err = fs.format(&sd);
        check_error(err);
    }

    char log_filename[64];
    create_log_filename(log_filename, sizeof(log_filename));

    FILE *fd = nullptr;

    auto open_new_log_file = [&]() {
        create_log_filename(log_filename, sizeof(log_filename));
        printf("Opening a new file, %s... ", log_filename);
        fd = fopen(log_filename, "w+");
        printf("%s\n", (!fd ? "Fail :(" : "OK")); 
        fprintf(fd, "Time (s),Pressure (hPa),Temperature (degC),Solar Current (mA),Capacitor Voltage (V),SoC (%%)\n");
    };

    open_new_log_file();
    read_calibration_data();

    while(1) {
        if (buttonPressed) {
            buttonPressed = false;
            fileMutex.lock();
            fclose(fd);
            delete_all_log_files();
            open_new_log_file();
            i=0;
            fileMutex.unlock();
        }

        int32_t raw_temp, raw_press;
        read_raw_data(raw_temp, raw_press);
        int32_t temperature = calc_temp(raw_temp);
        uint32_t pressure = calc_press(raw_press);
        float capacitor_voltage = capacitorVoltage.read() * REFERENCE_VOLTAGE; 
        float SoC = calculateSoC(capacitor_voltage);
        float solar_voltage = solarCurrent.read() * REFERENCE_VOLTAGE;
        float current = solar_voltage / Resistance;

        time(&currentTime);
        struct tm *current_tm = localtime(&currentTime);
        char timestamp[16];
        strftime(timestamp, sizeof(timestamp), "%H:%M:%S", current_tm);

        // Store data in the struct
        MeasurementData data;
        strncpy(data.timestamp, timestamp, sizeof(data.timestamp));
        data.pressure = pressure / 100;
        data.temperature = temperature / 100.0f + (temperature % 100) / 100.0f;
        data.solar_current = static_cast<int>(current * 1000);
        data.cap_voltage = capacitor_voltage;
        data.charge_level = static_cast<int>(SoC);
        
        // Log the data
        fileMutex.lock();
        printf("Writing data to file in CSV format, Line %d: %s, %d, %.1f, %d, %.1f, %d%%\n",
               i + 1,
               data.timestamp,
               data.pressure,
               data.temperature,
               data.solar_current,
               data.cap_voltage,
               data.charge_level);

        fprintf(fd, "%s,%d,%.1f,%f,%.1f,%d%%\n",
                data.timestamp,
                data.pressure,
                data.temperature,
                data.solar_current,
                data.cap_voltage,
                data.charge_level);
        fileMutex.unlock();
        i++;
        ThisThread::sleep_for(2s);
    }

    printf("Writing measurement values is done to a file (20/20) done.\n");

    printf("Closing file...");
    fclose(fd);
    printf(" done.\n");

    printf("Re-opening file read-only... ");
    fd = fopen(log_filename, "r");    
    printf("%s\n", (!fd ? "Fail :(" : "OK")); 

    printf("Dumping file to screen.\n");
    char buff[16] = {0};
    while (!feof(fd)) {
        int size = fread(&buff[0], 1, 15, fd);
        fwrite(&buff[0], 1, size, stdout);
    }
    printf("EOF.\n");

    printf("Closing file...");
    fclose(fd);
    printf(" done.\n");

    printf("Opening root directory... ");
    DIR *dir = opendir("/sd/");
    printf("%s\n", (!dir ? "Fail :(" : "OK"));

    struct dirent *de;
    printf("Printing all filenames:\n");
    while ((de = readdir(dir)) != NULL) {
        printf("  %s\n", &(de->d_name)[0]);
    }

    printf("Closing root directory... ");
    err = closedir(dir);
    check_error(err);

    printf("Unmounting... ");
    fflush(stdout);
    err = fs.unmount();
    check_error(err);
    
    printf("Mbed OS filesystem example done!\n");

    while (true) {
        if (buttonPressed) {
            buttonPressed = false;
            fileMutex.lock();
            delete_all_log_files();
            open_new_log_file();
            fileMutex.unlock();
        }
        ThisThread::sleep_for(1s);
    }
}
