/*
 * Light Meter Module for 4x5 Camera Light Meter
 * Handles EV calculations and exposure recommendations
 */

#ifndef LIGHT_METER_H
#define LIGHT_METER_H

#include <stddef.h>  // For size_t
#include <stdbool.h>  // For bool
#include "adc_reader.h"  // For led_measurement_t

// Metering modes
typedef enum {
    METERING_CENTER_WEIGHTED, // Default - center weighted average
    METERING_MATRIX,          // Matrix/evaluative - all LEDs with equal weight
    METERING_SPOT,            // Center spot only
    METERING_HIGHLIGHT        // Prioritize brightest areas
} metering_mode_t;

// Function prototypes
float calculate_ev(float lux_matrix[5][4], metering_mode_t mode);
float calculate_ev_from_detailed(led_measurement_t measurements[5][4], metering_mode_t mode);
float calculate_shutter_speed(float ev, int iso);
void get_exposure_recommendation(float ev, int iso, char *buffer, size_t buffer_size);
bool set_metering_mode(metering_mode_t mode);
const char* get_metering_mode_name(metering_mode_t mode);
metering_mode_t get_metering_mode_from_name(const char* name);

// Shutter speed calibration functions
bool set_shutter_speed_calibration(float calibration);
float get_shutter_speed_calibration(void);

#endif // LIGHT_METER_H
