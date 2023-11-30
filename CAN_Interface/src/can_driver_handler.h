#ifndef TWAI_DRIVER_HANDLER_H
#define TWAI _DRIVER_HANDLER_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>

// #warning driver/can.h is deprecated, please use driver/twai.h instead
// #include <driver/can.h>
#include <driver/twai.h>

/* CAN Config Datatype */
struct cannelloni_config_t {
    twai_timing_config_t bitrate;
    twai_mode_t  can_mode;
    int filter;
    int is_extended;
    int start_id;
    int end_id;
};

void setup_can_driver(cannelloni_config_t *config);
void set_bitrate(cannelloni_config_t *config, int bitrate);
void set_can_mode(cannelloni_config_t *config, int can_mode);
void settings_write(String key, cannelloni_config_t *config, void *value);

#endif
