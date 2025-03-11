/*
 * Light Meter Module for 4x5 Camera Light Meter
 * Implementation file
 */

#include "light_meter.h"
#include "esp_log.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "LIGHT_METER";

// Current metering mode
static metering_mode_t current_metering_mode = METERING_CENTER_WEIGHTED;

// K value for reflected light TTL meter (range 0-100)
static float k_value = 2.5f;

/**
 * Set the metering mode
 * Returns true if successful
 */
bool set_metering_mode(metering_mode_t mode) {
    // Validate mode
    if (mode < METERING_CENTER_WEIGHTED || mode > METERING_HIGHLIGHT) {
        ESP_LOGE(TAG, "Invalid metering mode: %d", mode);
        return false;
    }
    
    current_metering_mode = mode;
    ESP_LOGI(TAG, "Metering mode set to: %s", get_metering_mode_name(mode));
    return true;
}

/**
 * Convert metering mode to string name
 */
const char* get_metering_mode_name(metering_mode_t mode) {
    switch (mode) {
        case METERING_CENTER_WEIGHTED:
            return "center-weighted";
        case METERING_MATRIX:
            return "matrix";
        case METERING_SPOT:
            return "spot";
        case METERING_HIGHLIGHT:
            return "highlight";
        default:
            return "unknown";
    }
}

/**
 * Get metering mode from string name
 */
metering_mode_t get_metering_mode_from_name(const char* name) {
    if (name == NULL) {
        return METERING_CENTER_WEIGHTED;
    }
    
    if (strcasecmp(name, "center") == 0 || 
        strcasecmp(name, "central") == 0 || 
        strcasecmp(name, "center-weighted") == 0) {
        return METERING_CENTER_WEIGHTED;
    }
    else if (strcasecmp(name, "matrix") == 0 || 
             strcasecmp(name, "evaluative") == 0) {
        return METERING_MATRIX;
    }
    else if (strcasecmp(name, "spot") == 0) {
        return METERING_SPOT;
    }
    else if (strcasecmp(name, "highlight") == 0 || 
             strcasecmp(name, "highlights") == 0) {
        return METERING_HIGHLIGHT;
    }
    
    // Default
    return METERING_CENTER_WEIGHTED;
}

/**
 * Calculate Exposure Value (EV) from lux matrix
 * Uses different metering modes to process the readings
 */
float calculate_ev(float lux_matrix[5][4], metering_mode_t mode) {
    float total_lux = 0.0f;
    float count = 0.0f;
    
    // Apply metering mode-specific processing
    switch (mode) {
        case METERING_CENTER_WEIGHTED: {
            // Center-weighted metering gives more weight to center LEDs
            for (int row = 0; row < 5; row++) {
                for (int col = 0; col < 4; col++) {
                    // Central area (rows 1-3, cols 1-2)
                    float weight = 1.0f;
                    if (row >= 1 && row <= 3 && col >= 1 && col <= 2) {
                        weight = 2.0f;  // Double weight for center area
                    }
                    
                    total_lux += lux_matrix[row][col] * weight;
                    count += weight;
                }
            }
            break;
        }
        
        case METERING_MATRIX: {
            // Matrix metering uses all LEDs with equal weight
            for (int row = 0; row < 5; row++) {
                for (int col = 0; col < 4; col++) {
                    total_lux += lux_matrix[row][col];
                    count += 1.0f;
                }
            }
            break;
        }
        
        case METERING_SPOT: {
            // Spot metering uses only center LEDs (2,1) and (2,2)
            total_lux = lux_matrix[2][1] + lux_matrix[2][2];
            count = 2.0f;
            break;
        }
        
        case METERING_HIGHLIGHT: {
            // Highlight metering prioritizes the brightest readings
            float max_lux = 0.0f;
            float sum_top_quarter = 0.0f;
            float readings[20]; // All 20 readings in a flat array
            int idx = 0;
            
            // Collect all readings
            for (int row = 0; row < 5; row++) {
                for (int col = 0; col < 4; col++) {
                    readings[idx++] = lux_matrix[row][col];
                    if (lux_matrix[row][col] > max_lux) {
                        max_lux = lux_matrix[row][col];
                    }
                }
            }
            
            // Simple sort (insertion sort is fine for small arrays)
            for (int i = 1; i < 20; i++) {
                float key = readings[i];
                int j = i - 1;
                while (j >= 0 && readings[j] < key) {
                    readings[j + 1] = readings[j];
                    j--;
                }
                readings[j + 1] = key;
            }
            
            // Use top 25% (5 brightest readings)
            for (int i = 0; i < 5; i++) {
                sum_top_quarter += readings[i];
            }
            
            total_lux = sum_top_quarter;
            count = 5.0f;
            break;
        }
    }
    
    // Calculate average lux
    float average_lux = (count > 0) ? (total_lux / count) : 0.0f;
    
    // NEW EV calculation: EV = log₂((Lux × ISO) / (K × 100))
    float base_iso = 100.0f; // Default ISO value, will be adjusted in shutter speed calculation
    float ev = log2f((average_lux * (base_iso/100.0f)) / (k_value * 1.0f));
    
    ESP_LOGI(TAG, "Mode: %s, Average Lux: %.2f, Calculated EV: %.2f (K Method)", 
             get_metering_mode_name(mode), average_lux, ev);
    
    return ev;
}

/**
 * Calculate Exposure Value (EV) from detailed measurement results
 */
float calculate_ev_from_detailed(led_measurement_t measurements[5][4], metering_mode_t mode) {
    // Extract lux values into a simple matrix for processing
    float lux_matrix[5][4];
    
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 4; col++) {
            // Skip any saturated readings (ADC value near max)
            if (measurements[row][col].adc_value >= 4090) {
                ESP_LOGW(TAG, "Skipping saturated reading at row %d, col %d (ADC: %d)", 
                         row+1, col+1, measurements[row][col].adc_value);
                lux_matrix[row][col] = 0.0f; // Use 0 for saturated
                continue;
            }
            
            // Skip values below minimum reliable reading (10 lux per specs)
            if (measurements[row][col].lux < 10.0f) {
                ESP_LOGW(TAG, "Skipping too low reading at row %d, col %d (Lux: %.2f)", 
                         row+1, col+1, measurements[row][col].lux);
                lux_matrix[row][col] = 0.0f; // Use 0 for too low
                continue;
            }
            
            lux_matrix[row][col] = measurements[row][col].lux;
        }
    }
    
    // Calculate EV using the appropriate metering mode
    float ev = calculate_ev(lux_matrix, mode);
    
    // Clamp EV to reasonable range for photography (-6 to 20)
    ev = fmaxf(-6.0f, fminf(20.0f, ev));
    
    return ev;
}

/**
 * Calculate recommended shutter speed based on EV
 * Returns the shutter speed in seconds using the K Method
 */
float calculate_shutter_speed(float ev, int iso) {
    // K Method formula for shutter speed: Shutter Speed = 1 ÷ 2^EV
    // ISO is already factored into the EV calculation in calculate_ev()
    float shutter_speed = 1.0f / powf(2.0f, ev);
    
    ESP_LOGI(TAG, "EV: %.2f, ISO: %d, Shutter speed: %.4f seconds (K Method)", 
             ev, iso, shutter_speed);
    
    return shutter_speed;
}

/**
 * Get human-readable exposure recommendation for TTL metering
 */
void get_exposure_recommendation(float ev, int iso, char *buffer, size_t buffer_size) {
    // Get the shutter speed using the calculate_shutter_speed function
    float shutter_speed = calculate_shutter_speed(ev, iso);
    
    // Format shutter speed as a fraction if < 1 second
    if (shutter_speed >= 1.0f) {
        snprintf(buffer, buffer_size, "ISO %d, %.1f seconds (EV: %.1f)", 
                iso, shutter_speed, ev);
    } else {
        // Convert to fraction like 1/125
        int denominator = roundf(1.0f / shutter_speed);
        snprintf(buffer, buffer_size, "ISO %d, 1/%d (EV: %.1f)", 
                iso, denominator, ev);
    }
}

/**
 * Set the K value for reflected light
 * Returns true if successful
 */
bool set_k_value(float new_k_value) {
    // Validate K value (range 0-100)
    if (new_k_value < 0.0f || new_k_value > 100.0f) {
        ESP_LOGW(TAG, "K value out of range: %.2f (standard range is 0-100)", new_k_value);
        return false;
    }
    
    k_value = new_k_value;
    ESP_LOGI(TAG, "K value set to: %.2f", k_value);
    return true;
}

/**
 * Get the current K value
 */
float get_k_value(void) {
    return k_value;
}