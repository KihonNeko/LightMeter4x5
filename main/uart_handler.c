/*
 * UART Handler Module for 4x5 Camera Light Meter
 * Implementation file - Improved console version
 */

#include "uart_handler.h"
#include "esp_log.h"
#include "esp_console.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

static const char *TAG = "UART_HANDLER";

// Callbacks
static void (*iso_value_callback)(int) = NULL;
static void (*start_measurement_callback)(void) = NULL;
static void (*metering_mode_callback)(metering_mode_t) = NULL;
static void (*calibration_callback)(float) = NULL;

// Buffer for command input
static char cmd_line[UART_BUF_SIZE];
static uint8_t cmd_len = 0;

/**
 * Trim whitespace from a string
 */
static char* trim(char* str) {
    if (str == NULL) return NULL;
    
    // Trim leading space
    while(isspace((unsigned char)*str)) str++;
    
    if(*str == 0)  // All spaces?
        return str;
        
    // Trim trailing space
    char* end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    
    // Write new null terminator
    *(end+1) = 0;
    
    return str;
}

/**
 * Process a command string
 */
static void process_command(char *cmd) {
    ESP_LOGI(TAG, "Processing command: '%s'", cmd);
    
    // Trim whitespace
    cmd = trim(cmd);
    
    // Skip empty commands
    if (strlen(cmd) == 0) {
        return;
    }
    
    // Process commands
    if (strncmp(cmd, "config iso ", 11) == 0) {
        // Parse ISO value
        int iso = atoi(cmd + 11);
        ESP_LOGI(TAG, "ISO value parsed: %d", iso);
        
        if (iso > 0 && iso_value_callback != NULL) {
            iso_value_callback(iso);
            printf("ISO configured to: %d\n", iso);
        } else {
            printf("Error: Invalid ISO value\n");
        }
    } 
    else if (strncmp(cmd, "config type ", 12) == 0) {
        // Parse metering type
        const char* type_str = cmd + 12;
        ESP_LOGI(TAG, "Metering type parsed: '%s'", type_str);
        
        if (metering_mode_callback != NULL) {
            metering_mode_t mode = get_metering_mode_from_name(type_str);
            metering_mode_callback(mode);
            printf("Metering type configured to: %s\n", get_metering_mode_name(mode));
        } else {
            printf("Error: Metering mode callback not registered\n");
        }
    }
    else if (strncmp(cmd, "config calibration ", 19) == 0) {
        // Parse calibration value
        float calibration = atof(cmd + 19);
        ESP_LOGI(TAG, "Calibration value parsed: %.2f", calibration);
        
        if (calibration > 0.0f && calibration_callback != NULL) {
            calibration_callback(calibration);
            printf("Shutter speed calibration set to: %.2f\n", calibration);
        } else {
            printf("Error: Invalid calibration value (must be positive)\n");
        }
    }
    else if (strcmp(cmd, "start measure") == 0) {
        ESP_LOGI(TAG, "Start measure command received");
        
        if (start_measurement_callback != NULL) {
            start_measurement_callback();
            printf("Measurement started\n");
        } else {
            printf("Error: Measurement callback not registered\n");
        }
    }
    else if (strcmp(cmd, "help") == 0) {
        printf("\nAvailable commands:\n");
        printf("  config iso <value>         - Set ISO value (e.g., 100, 400, 800)\n");
        printf("  config type <mode>         - Set metering type (center, matrix, spot, highlight)\n");
        printf("  config calibration <value> - Set shutter speed calibration factor (default: 128.0)\n");
        printf("  start measure              - Start light measurement\n");
        printf("  help                       - Show this help\n");
        printf("  reset                      - Reset the device\n\n");
    }
    else if (strcmp(cmd, "reset") == 0) {
        printf("Resetting device...\n");
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_restart();
    }
    else {
        printf("Unknown command: '%s'. Type 'help' for available commands.\n", cmd);
    }
    
    // Print a prompt
    printf("> ");
}

/**
 * Initialize UART handler
 */
void uart_handler_init(void (*iso_callback)(int), void (*measure_callback)(void), 
                      void (*metering_callback)(metering_mode_t), void (*calib_callback)(float)) {
    // Store callbacks
    iso_value_callback = iso_callback;
    start_measurement_callback = measure_callback;
    metering_mode_callback = metering_callback;
    calibration_callback = calib_callback;
    
    ESP_LOGI(TAG, "UART handler initialized - using console for commands");
    
    // Initialize command buffer
    memset(cmd_line, 0, UART_BUF_SIZE);
    cmd_len = 0;
    
    // Print welcome message after a short delay
    vTaskDelay(pdMS_TO_TICKS(500));
    printf("\n\n=== 4x5 Camera Light Meter ===\n");
    printf("Type 'help' for available commands\n");
    printf("> ");
}

/**
 * Check for UART commands and process them
 */
void check_uart_commands(void) {
    // Read one character at a time from stdin
    char c;
    int res = fgetc(stdin);
    
    // If no character is available, return
    if (res == EOF) {
        return;
    }
    
    c = (char)res;
    
    // Echo character back to console (if not newline or carriage return)
    if (c != '\n' && c != '\r') {
        fputc(c, stdout);
    }
    
    // Process character
    if (c == '\n' || c == '\r') {
        // End of line, process command
        printf("\n");  // Echo newline
        cmd_line[cmd_len] = '\0';  // Null terminate
        process_command(cmd_line);
        
        // Reset command buffer
        memset(cmd_line, 0, UART_BUF_SIZE);
        cmd_len = 0;
    } 
    else if (c == 0x08 || c == 0x7F) {  // Backspace or Delete
        // Remove last character if buffer not empty
        if (cmd_len > 0) {
            cmd_len--;
            cmd_line[cmd_len] = '\0';
            // Echo backspace (erase last character)
            printf("\b \b");
        }
    }
    else {
        // Add character to buffer if not full
        if (cmd_len < UART_BUF_SIZE - 1) {
            cmd_line[cmd_len++] = c;
        }
    }
}
