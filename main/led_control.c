/*
 * LED Control Module for 4x5 Camera Light Meter
 * Implementation file
 */

 #include "led_control.h"
 #include "esp_log.h"
 #include "driver/gpio.h"
 
 static const char *TAG = "LED_CONTROL";
 
 /**
  * Initialize the LED control module
  */
 void led_control_init(void) {
     // Configure GPIO pins
     gpio_config_t io_conf = {};
     
     // Configure output pins
     io_conf.intr_type = GPIO_INTR_DISABLE;
     io_conf.mode = GPIO_MODE_OUTPUT;
     io_conf.pin_bit_mask = (1ULL << MULTIPLEX_0_PIN) | (1ULL << MULTIPLEX_1_PIN) | (1ULL << ENABLE_PIN);
     io_conf.pull_down_en = 0;
     io_conf.pull_up_en = 0;
     gpio_config(&io_conf);
     
     // Initial state: disabled, mux at 00
     gpio_set_level(ENABLE_PIN, 1);     // nENABLE is active low
     gpio_set_level(MULTIPLEX_0_PIN, 0);
     gpio_set_level(MULTIPLEX_1_PIN, 0);
     
     ESP_LOGI(TAG, "LED control module initialized");
 }
 
 /**
  * Select an LED based on row (1-5) and column (1-4)
  * This sets the appropriate multiplexer signals
  */
 void select_led(int row, int col) {
     // Validate inputs
     if (row < 1 || row > 5 || col < 1 || col > 4) {
         ESP_LOGE(TAG, "Invalid LED coordinates: row %d, col %d", row, col);
         return;
     }
     
     // Calculate mux settings based on the column (1-indexed)
     // Columns 1-4 correspond to multiplexer inputs 0-3
     int mux_setting = col - 1;
     
     // Set multiplexer select pins
     // MULTIPLEX_0 is the LSB, MULTIPLEX_1 is the MSB
     gpio_set_level(MULTIPLEX_0_PIN, mux_setting & 0x01);
     gpio_set_level(MULTIPLEX_1_PIN, (mux_setting >> 1) & 0x01);
     
     ESP_LOGD(TAG, "Selected LED at row %d, column %d", row, col);
 }
 
 /**
  * Enable or disable the measurement circuit
  * The nENABLE pin is active LOW
  */
 void enable_measurement(bool enable) {
     gpio_set_level(ENABLE_PIN, enable ? 0 : 1);
     
     ESP_LOGD(TAG, "Measurement circuit %s", enable ? "enabled" : "disabled");
 }