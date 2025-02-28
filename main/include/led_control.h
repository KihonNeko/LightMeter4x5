/*
 * LED Control Module for 4x5 Camera Light Meter
 * Handles multiplexer control for selecting LEDs
 */

 #ifndef LED_CONTROL_H
 #define LED_CONTROL_H
 
 #include <stdbool.h>
 
 // Pin definitions
 #define MULTIPLEX_0_PIN     GPIO_NUM_6  // IO6
 #define MULTIPLEX_1_PIN     GPIO_NUM_7  // IO7
 #define ENABLE_PIN          GPIO_NUM_3  // EN
 
 // Function prototypes
 void led_control_init(void);
 void select_led(int row, int col);
 void enable_measurement(bool enable);
 
 #endif // LED_CONTROL_H