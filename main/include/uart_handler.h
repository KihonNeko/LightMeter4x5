/*
 * UART Handler Module for 4x5 Camera Light Meter
 * Handles UART commands for configuration and measurement triggering
 */

#ifndef UART_HANDLER_H
#define UART_HANDLER_H

#include <stdbool.h>
#include "light_meter.h" // For metering_mode_t

// Buffer size for commands
#define UART_BUF_SIZE       256

// Function prototypes
void uart_handler_init(void (*iso_callback)(int), void (*measure_callback)(void), 
                      void (*metering_callback)(metering_mode_t), void (*calibration_callback)(float));
void check_uart_commands(void);

#endif // UART_HANDLER_H
