#pragma once

/**
 * @file wifi_provisioning.h
 * @brief Functions for WiFi provisioning with ESP32 captive portal.
 *
 * This module provides functionality for WiFi provisioning:
 * - Starts SoftAP and captive portal if no credentials are saved
 * - Starts HTTP server for entering WiFi SSID and password
 * - Connects to saved WiFi credentials if available
 * - Starts application tasks after successful connection
 */

/**
 * @brief Initialize and start WiFi provisioning / captive portal.
 *
 * If the device has no stored WiFi credentials, this function will:
 * - Start a SoftAP with SSID "ESP32_Setup"
 * - Start a DNS server for captive portal redirection
 * - Start an HTTP server serving the provisioning HTML page
 *
 * If credentials are stored, it will:
 * - Start WiFi in STA mode
 * - Attempt connection to the saved network
 * - Start application tasks (DHT11 reading, Firebase, buttons, etc.) after successful connection
 */
void wifi_provisioning_start(void);
