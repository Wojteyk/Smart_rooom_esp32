#ifndef _DHT_11
#define _DHT_11

#include "esp_log.h"
#include <driver/gpio.h>
#include <rom/ets_sys.h>
#include <stdio.h>
#include <string.h>

/**
 * @file dht11.h
 * @brief Functions and structures for reading temperature and humidity from DHT11 sensor
 *
 * Provides blocking functions for communication with the DHT11 sensor,
 * as well as a FreeRTOS task to send readings to Firebase.
 */

/**
 * @struct dht11_t
 * @brief Structure containing DHT11 sensor pin and readings.
 *
 * @var dht11_t::dht11_pin The GPIO pin connected to the DHT11 data line
 * @var dht11_t::temperature Last temperature reading in Celsius
 * @var dht11_t::humidity Last humidity reading in percentage
 */
typedef struct
{
    int dht11_pin;
    float temperature;
    float humidity;
} dht11_t;

/**
 * @brief Waits for the GPIO pin to reach the specified state.
 *
 * This function is blocking and will loop until the pin reaches the desired state
 * or the timeout is reached.
 *
 * @param dht11 DHT11 sensor structure
 * @param state GPIO state to wait for (0 or 1)
 * @param timeout Maximum time to wait (in microseconds)
 * @return Time waited in microseconds, or -1 if timeout occurs
 */
int wait_for_state(dht11_t dht11, int state, int timeout);

/**
 * @brief Drives the DHT11 pin low for the specified duration.
 *
 * Used to start communication with the sensor.
 *
 * @param dht11 DHT11 sensor structure
 * @param hold_time_us Duration to hold the pin low (in microseconds)
 */
void hold_low(dht11_t dht11, int hold_time_us);

/**
 * @brief Reads temperature and humidity from the DHT11 sensor.
 *
 * This function is blocking and performs a busy wait for communication.
 *
 * @note Wait at least 2 seconds between reads to avoid sensor errors
 * @param dht11 Pointer to the DHT11 sensor structure to update readings
 * @param connection_timeout Number of connection attempts before declaring timeout
 * @return 0 on success, -1 on failure (e.g., timeout or checksum error)
 */
int dht11_read(dht11_t* dht11, int connection_timeout);

/**
 * @brief FreeRTOS task that periodically reads DHT11 values and sends them to Firebase.
 *
 * The task reads the sensor every 5 minutes and logs temperature and humidity.
 *
 * @param pvParameters Task parameters (unused)
 */
void firebase_dht11_task(void* pvParameters);

/**
 * @brief Initializes the DHT11 sensor structure.
 *
 * Sets the GPIO pin used for the sensor.
 */
void dht11_init(void);

#endif //_DHT_11
