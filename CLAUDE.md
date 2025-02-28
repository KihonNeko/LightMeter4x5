# 4x5 Lightmeter Development Guidelines

## Build Commands
- Build project: `idf.py build`
- Clean project: `idf.py clean`
- Flash to device: `idf.py flash`
- Monitor serial output: `idf.py monitor`
- All-in-one command: `idf.py build flash monitor`
- Set target: `idf.py set-target esp32c3`

## Code Style Guidelines
- **Includes**: Group includes in logical blocks (standard libraries, ESP-IDF, custom headers)
- **Naming**: Use snake_case for variables and functions
- **Constants**: Prefix constants with `k` and use PascalCase (e.g., `kMaxValue`)
- **Error Handling**: Use ESP_ERROR_CHECK() for ESP-IDF functions that return esp_err_t
- **Logging**: Use ESP_LOG macros with appropriate tags and log levels
- **Comments**: Add header comments to files describing purpose; document complex functions
- **Types**: Use ESP-IDF types where appropriate (e.g., esp_err_t, gpio_num_t)
- **Memory**: Avoid dynamic memory allocation when possible; check malloc returns
- **Tasks**: Keep critical sections short; use appropriate stack sizes for FreeRTOS tasks