# 4x5 Camera Light Meter Project Documentation

## Project Overview

This document details a light meter project for a 4x5 camera using an ESP32-C3-WROOM-02 microcontroller. The light meter uses a 5×4 grid of 20 photodiode LEDs to measure light across the frame, providing accurate exposure information for large format photography with TTL (Through The Lens) metering.

## Hardware Architecture

### Microcontroller
- ESP32-C3-WROOM-02

### Sensor Array
- 20 photodiode LEDs arranged in a 5×4 grid (5 rows, 4 columns)
- Layout:
  ```
  LED1  LED2  LED3  LED4
  LED5  LED6  LED7  LED8
  LED9  LED10 LED11 LED12
  LED13 LED14 LED15 LED16
  LED17 LED18 LED19 LED20
  ```

### Multiplexer Setup
- 3× Texas Instruments TS3A5017PWR multiplexers
- Connected to GPIO6 (MULTIPLEX_0) and GPIO7 (MULTIPLEX_1) for selection
- Enable pin connected to GPIO5 (nENABLE - active low)

### ADC Connections
- Row 1 (LEDs 1-4): GPIO0 (ADC_CHANNEL_0)
- Row 2 (LEDs 5-8): GPIO1 (ADC_CHANNEL_1)
- Row 3 (LEDs 9-12): GPIO2 (ADC_CHANNEL_2)
- Row 4 (LEDs 13-16): GPIO3 (ADC_CHANNEL_3)
- Row 5 (LEDs 17-20): GPIO4 (ADC_CHANNEL_4)

### Photodiode Characteristics
Precision with 12-bit ADC for L-Gain Photographic Light Meter:
- **Sensitivity Formula**: Viout = 0.0057 × 10^-6 × Ev × R1
  - Where Viout is the output voltage
  - Ev is the illuminance in lux
  - R1 is the load resistor value in ohms

### Load Resistor
- **Selected value**: 1.3 kOhm (1,300 ohms)
- This value is selected to provide approximately 2.96V at 400,000 lux
- With 11 ohm RDSon added, total resistance is ~1,311 ohms
- Theoretical calculation: 1,316 ohms would give exactly 3V at 400,000 lux

### ADC Specifications
- 12-bit resolution (0-4095)
- 3.3V reference voltage
- At 400,000 lux with 1.3k ohm resistor: ADC reads approximately 3,684 (90% of full scale)

## Software Architecture

### Core Components
1. **led_control** - Manages multiplexer control for selecting LEDs
2. **adc_reader** - Handles ADC measurements and converting to lux
3. **light_meter** - Calculates exposure values and suggestions
4. **uart_handler** - Processes user commands

### Development Environment
- ESP-IDF v5.4
- C programming language
- VS Code with ESP-IDF extension

### Lux Conversion
The exact formula implemented for converting ADC readings to lux:
```c
// Get voltage (Viout)
float voltage = get_voltage_from_adc(adc_value);
    
// Constants from formula
float sensitivity = 0.0057e-6f; // 0.0057 × 10^-6
    
// Calculate illuminance (lux) directly using the formula
float lux = voltage / (sensitivity * RLOAD_OHM);
```

### Exposure Value Calculation
- Formula: EV = log₂(lux/2.5)
- Center-weighted metering is implemented
- Skip saturated readings (ADC values near maximum)
- Skip readings below 10 lux (minimum reliable threshold)
- Clamp EV to photography range (-6 to 20)

### Shutter Speed Calculation
- The device outputs the EV value based on TTL metering
- ISO setting is used for proper exposure calculation
- Using standard exposure formulas, EV can be used with any aperture/shutter combination

## User Interface

### UART Commands
The light meter accepts the following commands at 115200 baud:

1. Set ISO sensitivity:
   ```
   config iso 100
   ```

2. Start light measurement:
   ```
   start measure
   ```

3. Display help information:
   ```
   help
   ```

4. Reset the device:
   ```
   reset
   ```

### Output Format
After a measurement, the device outputs:

1. A detailed table showing for each LED:
   - ADC value (0-4095)
   - Voltage reading (V)
   - Calculated illuminance (lux)

2. Exposure information:
   - EV (Exposure Value)
   - ISO setting
   - The EV can be used with standard exposure calculators or tables

## Performance Specifications

### Light Metering Range
- **Minimum measurable light**: ~10 lux
- **Maximum measurable light**: ~400,000 lux
- **Effective Dynamic Range**: ~15 stops

### Precision
- **12-bit ADC Resolution**: 3.3V / 4096 steps = 0.806 mV per step
- **Practical EV Resolution**: 0.1 EV 

## Example Output

```
================= DETAILED MEASUREMENTS =================
    | Column 1      | Column 2      | Column 3      | Column 4      |
Row | ADC  V    Lux | ADC  V    Lux | ADC  V    Lux | ADC  V    Lux |
----+---------------+---------------+---------------+---------------+
 1  |  873 0.61V 81603.0 |  874 0.62V 81696.8 |  873 0.61V 81603.0 |  873 0.61V 81603.0 |
 2  |  857 0.60V 80103.0 |  856 0.60V 80009.2 |  855 0.60V 79915.5 |  856 0.60V 80009.2 |
 3  |  809 0.57V 75632.7 |  810 0.57V 75726.4 |  810 0.57V 75726.4 |  810 0.57V 75726.4 |
 4  |  860 0.61V 80384.2 |  857 0.60V 80103.0 |  858 0.60V 80196.7 |  858 0.60V 80196.7 |
 5  | 2122 1.71V 198322.4 | 2122 1.71V 198322.4 | 2122 1.71V 198322.4 | 2122 1.71V 198322.4 |
===========================================================

Measured EV: 14.2 at ISO 100
```

## Configurable Parameters

These parameters can be adjusted based on photodiode characteristics and desired sensitivity:

1. **Load resistor (RLOAD_OHM)**: 
   - Currently set to 1.3 kOhm (1,300 ohms)
   - Selected to provide ~2.96V at 400,000 lux

2. **Photodiode sensitivity constant**: 
   - Currently 0.0057 × 10^-6

3. **Reference voltage**:
   - 3.3V (ESP32-C3 ADC reference)

4. **ISO settings**:
   - Configurable via UART command
   - Affects the EV calculation for proper exposure

## Future Enhancements

1. Add multiple metering modes (spot, average, highlight)
2. Calibration routine for improved accuracy
3. Save and recall settings from NVS (non-volatile storage)
4. WiFi interface for remote control
5. Display for reading values without a computer

---

This documentation reflects the latest implementation of the 4x5 camera light meter project as of February 2025.