#pragma once

#include "esp_err.h"
#include "esp_http_client.h"

/**
 * @file firebase.h
 * @brief Header file for the Firebase Realtime Database client library for ESP-IDF.
 * * Provides generic functions for sending (PUT) and receiving (GET) data
 * to a Firebase Realtime Database endpoint using the ESP HTTP Client.
 */
void firebase_switch_stream_task(void* pvParameters);
// --------------------------------------------------------------------------
// --- PUT Implementation Functions (Hidden behind generic macro) ----------
// --------------------------------------------------------------------------

/**
 * @brief Writes a single floating-point value to the Realtime Database.
 * * @param path The relative path in the database (e.g., "devices/temp").
 * @param value The float value to be written. This is sent as a raw number payload.
 * @return esp_err_t Returns ESP_OK on successful HTTP transaction (status 2xx), or an error code
 * otherwise.
 */
esp_err_t firebase_put_float_impl(const char* path, float value);

/**
 * @brief Writes a single integer value to the Realtime Database.
 * * @param path The relative path in the database (e.g., "devices/counter").
 * @param value The integer value to be written. This is sent as a raw number payload.
 * @return esp_err_t Returns ESP_OK on successful HTTP transaction (status 2xx), or an error code
 * otherwise.
 */
esp_err_t firebase_put_int_impl(const char* path, int value);

/**
 * @brief Writes a single boolean value to the Realtime Database.
 * * @param path The relative path in the database (e.g., "devices/status").
 * @param value The boolean value ('true' or 'false') to be written. Sent as JSON boolean literal.
 * @return esp_err_t Returns ESP_OK on successful HTTP transaction (status 2xx), or an error code
 * otherwise.
 */
esp_err_t firebase_put_bool_impl(const char* path, bool value);

/**
 * @brief Writes a single string value to the Realtime Database.
 * * @param path The relative path in the database (e.g., "devices/message").
 * @param value The string value to be written. This is automatically enclosed in quotes (e.g.,
 * "Hello").
 * @return esp_err_t Returns ESP_OK on successful HTTP transaction (status 2xx), or an error code
 * otherwise.
 */
esp_err_t firebase_put_string_impl(const char* path, const char* value);

// --------------------------------------------------------------------------
// --- GENERIC PUT MACRO ----------------------------------------------------
// --------------------------------------------------------------------------

/**
 * @brief Writes a generic value (float, int, bool, string) to the Realtime Database.
 * * This is a C11 _Generic macro that selects the appropriate type-specific
 * implementation function (e.g., firebase_put_float_impl) at compile time.
 * * @param path The target path in the database (e.g., "devices/state").
 * @param value The value to be sent (float, int, bool, or char*).
 * @return esp_err_t Returns ESP_OK on success.
 */
#define firebase_put(path, value)                                                                  \
    _Generic((value),                                                                              \
        float: firebase_put_float_impl,                                                            \
        int: firebase_put_int_impl,                                                                \
        bool: firebase_put_bool_impl,                                                              \
        const char*: firebase_put_string_impl,                                                     \
        char*: firebase_put_string_impl)(path, value)

// --------------------------------------------------------------------------
// --- GET FUNCTION ---------------------------------------------------------
// --------------------------------------------------------------------------

/**
 * @brief Reads the JSON data from the Realtime Database at a given path.
 * * The data received from Firebase is typically raw JSON and is stored
 * in the output buffer.
 *
 * @param path The relative path in the database (e.g., "config/settings").
 * @param out_buf Pointer to the buffer where the received data will be stored.
 * @param out_len The size of the output buffer (out_buf).
 * @return esp_err_t Returns ESP_OK on successful HTTP transaction and data read, or an error code
 * otherwise.
 */
esp_err_t firebase_get(const char* path, char* out_buf, size_t out_len);