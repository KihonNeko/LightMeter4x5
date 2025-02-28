# CMakeLists for main component
idf_component_register(
    SRCS "main.c"
         "led_control.c"
         "adc_reader.c"
         "light_meter.c"
         "uart_handler.c"
    INCLUDE_DIRS "include"
)
