/*
 * ADC Reader Module for 4x5 Camera Light Meter
 * Handles ADC measurements and conversion to lux
 */

 #ifndef ADC_READER_H
 #define ADC_READER_H
 
 #include "esp_adc/adc_oneshot.h"
 
 // ADC pin definitions using GPIO pins
 // Note: ESP32-C3 only supports ADC1 with channels 0-4
 #define ADC_LED14_GPIO       0   // For LEDs 1-4, using GPIO 0
 #define ADC_LED58_GPIO       1   // For LEDs 5-8, using GPIO 1
 #define ADC_LED912_GPIO      2   // For LEDs 9-12, using GPIO 2
 #define ADC_LED1316_GPIO     3   // For LEDs 13-16, using GPIO 3
 #define ADC_LED1720_GPIO     4   // For LEDs 17-20, using GPIO 4
 
 // Structure to store detailed measurement results
 typedef struct {
     int adc_value;
     float voltage;
     float lux;
 } led_measurement_t;
 
 // Function prototypes
 void adc_reader_init(void);
 int read_adc_for_led(int row, int col);
 float convert_to_lux(int adc_value);
 void measure_all_leds(float lux_matrix[5][4]);
 
 // New function for detailed measurements
 void measure_all_leds_detailed(led_measurement_t measurements[5][4]);
 
 #endif // ADC_READER_H