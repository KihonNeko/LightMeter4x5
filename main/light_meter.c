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

// Calibration factor for shutter speed calculation
// This is a multiplier applied to the calculated shutter speed
// A value of 128.0 (2^7) means the shutter speed will be 128x longer
// For EV 14.36 at ISO 100, this gives approximately 1/200 sec
static float shutter_speed_calibration = 0.0f;

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
    
    // EV calculation from lux: EV = log2(lux/2.5)
    float ev = log2f(average_lux / 2.5f);
    
    ESP_LOGI(TAG, "Mode: %s, Average Lux: %.2f, Calculated EV: %.2f", 
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
 * Calculate recommended shutter speed based on EV and ISO
 * Returns the shutter speed in seconds
 * 
 * Note: This function applies a calibration factor to get reasonable shutter speeds
 * for the specific light sensor used in this device. The EV value itself is not
 * modified, only the shutter speed calculation.
 */
float calculate_shutter_speed(float ev, int iso) {
    // Standard formula for shutter speed from EV: t = 2^(-EV) * (100/ISO)
    float uncalibrated_speed = powf(2.0f, -ev) * (100.0f / iso);
    
    // Apply the calibration factor to get more reasonable shutter speeds
    // For EV 14.36 at ISO 100, we want approximately 1/200 sec
    float shutter_speed = uncalibrated_speed * shutter_speed_calibration;
    
    ESP_LOGI(TAG, "EV: %.2f, ISO: %d, Uncalibrated: %.6f sec, Calibrated shutter speed: %.4f seconds", 
             ev, iso, uncalibrated_speed, shutter_speed);
    
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
 * Set the shutter speed calibration factor
 * Returns true if successful
 */
bool set_shutter_speed_calibration(float calibration) {
    // Validate calibration factor (must be positive)
    if (calibration <= 0.0f) {
        ESP_LOGE(TAG, "Invalid calibration factor: %.2f (must be positive)", calibration);
        return false;
    }
    
    shutter_speed_calibration = calibration;
    ESP_LOGI(TAG, "Shutter speed calibration set to: %.2f", shutter_speed_calibration);
    return true;
}

/**
 * Get the current shutter speed calibration factor
 */
float get_shutter_speed_calibration(void) {
    return shutter_speed_calibration;
}
