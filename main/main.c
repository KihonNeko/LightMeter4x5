/*
 * 4x5 Camera Light Meter
 * Main program file for ESP32-C3-WROOM-02 using ESP-IDF
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"  // Updated to use the new ADC API
#include "driver/uart.h"

#include "led_control.h"
#include "adc_reader.h"
#include "light_meter.h"
#include "uart_handler.h"

static const char *TAG = "LIGHT_METER";

// Global variables
volatile bool start_measurement = false;
int current_iso = 100; // Default ISO value
metering_mode_t current_metering_mode = METERING_CENTER_WEIGHTED; // Default metering mode
led_measurement_t led_measurements[5][4]; // Detailed measurements for all 20 LEDs

// Function prototypes
void app_main(void);
void set_iso_value(int iso);
void update_metering_mode(metering_mode_t mode);
void update_calibration(float calibration);
void trigger_measurement(void);
void print_detailed_measurements(void);

void app_main(void)
{
    // Initialize logging
    esp_log_level_set(TAG, ESP_LOG_INFO);
    ESP_LOGI(TAG, "4x5 Camera Light Meter Starting...");
    
    // Initialize LED control
    led_control_init();
    
    // Initialize ADC reader
    adc_reader_init();
    
    // Initialize UART handler for commands
    uart_handler_init(set_iso_value, trigger_measurement, update_metering_mode, update_calibration);
    
    ESP_LOGI(TAG, "Initialization Complete. Ready for measurements.");

    // Main loop
    while (1) {
        // Check for UART commands
        check_uart_commands();
        
        // If measurement is triggered
        if (start_measurement) {
            ESP_LOGI(TAG, "Starting light measurement with %s metering...", 
                    get_metering_mode_name(current_metering_mode));
            
            // Measure all LEDs with detailed values
            measure_all_leds_detailed(led_measurements);
            
            // Calculate exposure values using the current metering mode
            float ev = calculate_ev_from_detailed(led_measurements, current_metering_mode);
            float shutter_speed = calculate_shutter_speed(ev, current_iso);
            
            // Display results
            ESP_LOGI(TAG, "Light measurement completed. EV: %.2f, ISO: %d, Recommended Shutter Speed: %.4f", 
                          ev, current_iso, shutter_speed);

            // Print detailed measurements
            print_detailed_measurements();
            
            // Print exposure recommendation (TTL meter - no aperture)
            char buffer[100];
            get_exposure_recommendation(ev, current_iso, buffer, sizeof(buffer));
            printf("\nExposure recommendation: %s\n", buffer);
            printf("Metering mode: %s\n\n", get_metering_mode_name(current_metering_mode));
            printf("> ");  // Reprint prompt
            
            // Reset flag
            start_measurement = false;
        }
        
        // Small delay to prevent CPU hogging
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Callback function for UART "config iso" command
void set_iso_value(int iso) {
    current_iso = iso;
    ESP_LOGI(TAG, "ISO configured to: %d", current_iso);
}

// Callback function for UART "config type" command
void update_metering_mode(metering_mode_t mode) {
    current_metering_mode = mode;
    ESP_LOGI(TAG, "Metering mode configured to: %s", get_metering_mode_name(mode));
}

// Callback function for UART "config calibration" command
void update_calibration(float calibration) {
    set_shutter_speed_calibration(calibration);
    ESP_LOGI(TAG, "Shutter speed calibration set to: %.2f", calibration);
}

// Callback function for UART "start measure" command
void trigger_measurement(void) {
    start_measurement = true;
}

// Print detailed measurements including ADC, voltage, and lux values
void print_detailed_measurements(void) {
    printf("\n================= DETAILED MEASUREMENTS =================\n");
    printf("    | Column 1      | Column 2      | Column 3      | Column 4      |\n");
    printf("Row | ADC  V    Lux | ADC  V    Lux | ADC  V    Lux | ADC  V    Lux |\n");
    printf("----+---------------+---------------+---------------+---------------+\n");
    
    for (int row = 0; row < 5; row++) {
        printf(" %d  |", row + 1);
        
        for (int col = 0; col < 4; col++) {
            printf(" %4d %.2fV %5.1f |", 
                led_measurements[row][col].adc_value, 
                led_measurements[row][col].voltage, 
                led_measurements[row][col].lux);
        }
        
        printf("\n");
    }
    
    printf("===========================================================\n");
}
