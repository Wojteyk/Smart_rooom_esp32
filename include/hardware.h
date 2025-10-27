#pragma once

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/**
 * @brief Sets the relay state based on a JSON payload.
 *
 * This function parses the given JSON payload to determine
 * the desired relay state (e.g., ON or OFF) and updates
 * the GPIO output accordingly.
 *
 * @param json_payload A JSON-formatted string containing the relay state information.
 *                     Example: {"relay": "on"}
 */
void set_relay_state(const char* json_payload);

/**
 * @brief Relay hardware initialization.
 *
 * Configures the relay GPIO pin as output and sets initial state to OFF.
 */
void relay_init(void);

/**
 * @brief Initialize PC switch (button) input.
 *
 * Configures the button GPIO pin with interrupt on falling edge,
 * creates the event queue, and attaches the ISR handler.
 */
void pc_switch_init(void);

/**
 * @brief FreeRTOS task to handle button press events.
 *
 * Listens to the button GPIO event queue, applies debouncing,
 * toggles the relay state, and updates Firebase.
 *
 * @param pvParameters Task parameters (unused)
 */
void button_handler_task(void* pvParameters);
