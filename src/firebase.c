#include <stdbool.h>
#include <string.h>

#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "firebase.h"

typedef esp_http_client_handle_t firebase_stream_handle_t;
#define MAX_RETRY_NUM 5
#define RETRY_DELAY_MS 500
static const char* TAG = "firebase_client";

const char* FIREBASE_BASE_URL
    = "https://espbackendapp-default-rtdb.europe-west1.firebasedatabase.app/";

extern void set_relay_state(const char* json_payload);

// Internal helper to perform HTTP PUT with retries
static esp_err_t
_firebase_put_http(const char* path, const char* json_payload)
{
    int retry_cnt = 0;
    char url[256];
    esp_err_t err = ESP_FAIL;
    snprintf(url, sizeof(url), "%s/%s.json", FIREBASE_BASE_URL, path);

    do
    {
        esp_http_client_config_t config = {
            .url = url,
            .method = HTTP_METHOD_PUT,
            .crt_bundle_attach = esp_crt_bundle_attach,
        };

        esp_http_client_handle_t client = esp_http_client_init(&config);
        esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_post_field(client, json_payload, strlen(json_payload));

        err = esp_http_client_perform(client);
        if (err == ESP_OK)
        {
            int status_code = esp_http_client_get_status_code(client);

            if (status_code >= 200 && status_code < 300)
            {
                ESP_LOGI(TAG, "PUT success, status=%d", status_code);
                err = ESP_OK;
            }
            else
            {
                ESP_LOGW(TAG, "PUT failed (HTTP status %d)", status_code);
                err = ESP_FAIL;
            }
        }
        else
        {
            ESP_LOGE(TAG, "PUT failed (transport): %s", esp_err_to_name(err));
        }

        esp_http_client_cleanup(client);

        if (err != ESP_OK && retry_cnt < MAX_RETRY_NUM)
        {
            vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
        }

        retry_cnt++;
    } while (err != ESP_OK && retry_cnt < MAX_RETRY_NUM);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "PUT FAILED after %d attempts.", MAX_RETRY_NUM);
    }

    return err;
}

esp_err_t
firebase_put_float_impl(const char* path, float value)
{
    char payload_str[32];
    snprintf(payload_str, sizeof(payload_str), "%.2f", value);
    return _firebase_put_http(path, payload_str);
}

esp_err_t
firebase_put_int_impl(const char* path, int value)
{
    char payload_str[32];
    snprintf(payload_str, sizeof(payload_str), "%d", value);
    return _firebase_put_http(path, payload_str);
}

esp_err_t
firebase_put_bool_impl(const char* path, bool value)
{
    char payload_str[8];
    const char* bool_string = value ? "true" : "false";
    snprintf(payload_str, sizeof(payload_str), "%s", bool_string);
    return _firebase_put_http(path, payload_str);
}

esp_err_t
firebase_put_string_impl(const char* path, const char* value)
{
    return _firebase_put_http(path, value);
}

firebase_stream_handle_t
firebase_start_stream(const char* path)
{
    char url[256];
    snprintf(url, sizeof(url), "%s/%s.json", FIREBASE_BASE_URL, path);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 60000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL)
    {
        ESP_LOGE(TAG, "Failed to initialize HTTP client for stream");
        return NULL;
    }

    esp_http_client_set_header(client, "Accept", "text/event-stream");

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Stream connection failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return NULL;
    }

    int headers_len = esp_http_client_fetch_headers(client);
    if (headers_len < 0 || esp_http_client_get_status_code(client) != 200)
    {
        ESP_LOGE(TAG, "Stream header fetch failed or bad status: %d",
                 esp_http_client_get_status_code(client));
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return NULL;
    }

    ESP_LOGI(TAG, "Firebase stream established successfully.");
    return client;
}

void
firebase_switch_stream_task(void* pvParameters)
{
    (void)pvParameters;

    firebase_stream_handle_t stream_handle = NULL;
    const char* path = "CONTROLS/pc_switch";

    char stream_buffer[256] = {0};
    int current_pos = 0;

    while (true)
    {
        if (stream_handle == NULL)
        {
            ESP_LOGW(TAG, "Attempting to start Firebase stream...");
            stream_handle = firebase_start_stream(path);
            if (stream_handle == NULL)
            {
                vTaskDelay(pdMS_TO_TICKS(5000)); // Retry after 5s if failed
                continue;
            }
        }

        int read_len = esp_http_client_read(stream_handle, stream_buffer + current_pos, 1);

        if (read_len > 0)
        {
            if (stream_buffer[current_pos] == '\n')
            {
                stream_buffer[current_pos] = '\0';

                if (strstr(stream_buffer, "data:") != NULL)
                {
                    char* data_ptr = strchr(stream_buffer, '{');
                    if (data_ptr != NULL)
                    {
                        set_relay_state(data_ptr);
                    }
                }
                current_pos = 0;
                memset(stream_buffer, 0, sizeof(stream_buffer));
            }
            else if (current_pos < sizeof(stream_buffer) - 1)
            {
                current_pos++;
            }
            else
            {
                ESP_LOGE(TAG, "Stream buffer overflow, resetting.");
                current_pos = 0;
            }
        }
        else if (read_len == 0)
        {
            ESP_LOGW(TAG, "Stream closed by server, reconnecting...");
            esp_http_client_close(stream_handle);
            esp_http_client_cleanup(stream_handle);
            stream_handle = NULL;
        }
        else
        {
            ESP_LOGE(TAG, "Stream read error: %s", esp_err_to_name((esp_err_t)read_len));
            esp_http_client_close(stream_handle);
            esp_http_client_cleanup(stream_handle);
            stream_handle = NULL;
        }
    }
}
