/*
 * ADC Reader Module for 4x5 Camera Light Meter
 * Implementation file - ESP32-C3 specific version
 */

 #include "adc_reader.h"
 #include "led_control.h"
 #include "esp_log.h"
 #include "driver/gpio.h"
 #include "esp_adc/adc_oneshot.h"
 #include "esp_adc/adc_cali.h"
 #include "esp_adc/adc_cali_scheme.h"
 #include "freertos/FreeRTOS.h"
 #include "freertos/task.h"
 #include <math.h>
 
 static const char *TAG = "ADC_READER";
 
 // ADC handles
 static adc_oneshot_unit_handle_t adc1_handle;
 static adc_cali_handle_t adc1_cali_handle = NULL;
 static bool do_calibration = true;
 
 // Mapping from GPIO to ADC channels for ESP32-C3
 // ESP32-C3 only supports ADC1 with channels 0-4
 static adc_channel_t gpio_to_adc_channel(int gpio_num) {
     // This mapping is specifically for ESP32-C3
     switch (gpio_num) {
         case 0: return ADC_CHANNEL_0;
         case 1: return ADC_CHANNEL_1;
         case 2: return ADC_CHANNEL_2;
         case 3: return ADC_CHANNEL_3;
         case 4: return ADC_CHANNEL_4;
         default: 
             ESP_LOGE(TAG, "Invalid GPIO for ADC on ESP32-C3: %d", gpio_num);
             return ADC_CHANNEL_0; // Return channel 0 as fallback
     }
 }
 
 // Constants for lux conversion
 #define RLOAD_OHM 1300  // 739 + 11 Ohm RDSon (using 750 ohm standard value)
 
 /**
  * Initialize the ADC reader module
  */
 void adc_reader_init(void) {
     // ADC initialization
     adc_oneshot_unit_init_cfg_t init_config = {
         .unit_id = ADC_UNIT_1,
     };
     ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc1_handle));
     
     // ADC configuration for each channel
     adc_oneshot_chan_cfg_t config = {
         .atten = ADC_ATTEN_DB_12,  // 0-3.3V (using up-to-date attenuation value)
         .bitwidth = ADC_BITWIDTH_12,  // 12-bit resolution (0-4095)
     };
     
     // Map GPIO pins to ADC channels and configure them
     adc_channel_t channels[5];
     channels[0] = gpio_to_adc_channel(ADC_LED14_GPIO);
     channels[1] = gpio_to_adc_channel(ADC_LED58_GPIO);
     channels[2] = gpio_to_adc_channel(ADC_LED912_GPIO);
     channels[3] = gpio_to_adc_channel(ADC_LED1316_GPIO);
     channels[4] = gpio_to_adc_channel(ADC_LED1720_GPIO);
     
     // Configure each ADC channel
     for (int i = 0; i < 5; i++) {
         ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, channels[i], &config));
         ESP_LOGI(TAG, "Configured ADC channel %d", channels[i]);
     }
     
     // Calibration setup
     if (do_calibration) {
         adc_cali_curve_fitting_config_t cali_config = {
             .unit_id = ADC_UNIT_1,
             .atten = ADC_ATTEN_DB_12,
             .bitwidth = ADC_BITWIDTH_12,
         };
         ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_config, &adc1_cali_handle));
     }
     
     ESP_LOGI(TAG, "ADC reader module initialized");
 }
 
 /**
  * Read ADC value for specific LED based on row and column
  */
 int read_adc_for_led(int row, int col) {
     // Select the proper LED via multiplexers
     select_led(row, col);
     
     // Small delay to allow multiplexer to settle
     vTaskDelay(pdMS_TO_TICKS(1));
     
     // Enable the measurement circuit
     enable_measurement(true);
     
     // Additional delay for circuit to stabilize
     vTaskDelay(pdMS_TO_TICKS(10));
     
     // Determine which ADC channel to read based on the row
     // For ESP32-C3 we need to map our GPIO pins to the available channels (0-4)
     adc_channel_t adc_channel;
     
     switch (row) {
         case 1:
             adc_channel = gpio_to_adc_channel(ADC_LED14_GPIO);
             break;
         case 2:
             adc_channel = gpio_to_adc_channel(ADC_LED58_GPIO);
             break;
         case 3:
             adc_channel = gpio_to_adc_channel(ADC_LED912_GPIO);
             break;
         case 4:
             adc_channel = gpio_to_adc_channel(ADC_LED1316_GPIO);
             break;
         case 5:
             adc_channel = gpio_to_adc_channel(ADC_LED1720_GPIO);
             break;
         default:
             ESP_LOGE(TAG, "Invalid row for ADC reading: %d", row);
             return 0;
     }
     
     // Read ADC value
     int adc_raw;
     ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, adc_channel, &adc_raw));
     
     // Disable measurement circuit
     enable_measurement(false);
     
     ESP_LOGD(TAG, "LED at row %d, column %d, ADC value: %d", row, col, adc_raw);
     
     return adc_raw;
 }
 
 /**
  * Get the voltage from an ADC value
  */
 float get_voltage_from_adc(int adc_value) {
    if (adc1_cali_handle && adc_value < 4000) {
        // Only use calibration for non-saturated values
        int voltage_mv;
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, adc_value, &voltage_mv));
        return voltage_mv / 1000.0f; // Convert mV to V
    } else {
        // Manual conversion with 3.3V reference, 12-bit ADC (0-4095)
        float voltage = (adc_value * 3.3f) / 4095.0f;
        
        // If we're at max ADC reading, ensure we report max voltage
        if (adc_value >= 4090) {
            voltage = 3.3f;
        }
        
        return voltage;
    }
}
 
/**
 * Convert ADC reading to lux based on the exact formula:
 * Viout = 0.0057 × 10^-6 × Ev × R1
 * 
 * Rearranged to solve for Ev (illuminance in lux):
 * Ev = Viout / (0.0057 × 10^-6 × R1)
 */
float convert_to_lux(int adc_value) {
    // Get voltage (Viout)
    float voltage = get_voltage_from_adc(adc_value);
    
    // Constants from formula
    float sensitivity = 0.0057e-6f; // 0.0057 × 10^-6
    
    // Calculate illuminance (lux) directly using the formula
    float lux = voltage / (sensitivity * RLOAD_OHM);
    
    // Log the values for debugging
    ESP_LOGD(TAG, "ADC: %d, Voltage: %.4fV, Lux: %.2f", 
             adc_value, voltage, lux);
    
    return lux;
}
 
 /**
  * Measure all LEDs and populate the lux matrix
  */
 void measure_all_leds(float lux_matrix[5][4]) {
     ESP_LOGI(TAG, "Measuring all LEDs...");
     
     for (int row = 1; row <= 5; row++) {
         for (int col = 1; col <= 4; col++) {
             // Read ADC value
             int adc_value = read_adc_for_led(row, col);
             
             // Convert to lux and store in matrix
             // Note: Arrays are 0-indexed, but our row/col are 1-indexed
             lux_matrix[row-1][col-1] = convert_to_lux(adc_value);
             
             // Short delay between measurements
             vTaskDelay(pdMS_TO_TICKS(50));
         }
     }
     
     ESP_LOGI(TAG, "All LED measurements completed");
 }
 
 /**
  * Measure all LEDs with detailed values including ADC, voltage, and lux
  */
 void measure_all_leds_detailed(led_measurement_t measurements[5][4]) {
     ESP_LOGI(TAG, "Starting detailed measurements of all LEDs...");
     
     for (int row = 1; row <= 5; row++) {
         for (int col = 1; col <= 4; col++) {
             // Read ADC value
             int adc_value = read_adc_for_led(row, col);
             
             // Store ADC value
             measurements[row-1][col-1].adc_value = adc_value;
             
             // Convert to voltage and store
             measurements[row-1][col-1].voltage = get_voltage_from_adc(adc_value);
             
             // Convert to lux and store
             measurements[row-1][col-1].lux = convert_to_lux(adc_value);
             
             // Short delay between measurements
             vTaskDelay(pdMS_TO_TICKS(50));
         }
     }
     
     ESP_LOGI(TAG, "All detailed LED measurements completed");
 }