#include "dns_server.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#include "dht11.h"
#include "firebase.h"
#include "hardware.h"
#include "provisionig_html.h"
#include "wifi_provisioning.h"

static const char* TAG = "wifi_prov";

#define MAX_LISTEN_INTERVAL 10
#define MAX_RETRY_NUM 5
static const char* AP_SSID = "ESP32_Setup";
static const char* AP_PASS = "";
bool tasks_started = false;

// Forward declarations
static httpd_handle_t start_webserver(void);
static void wifi_event_handler(void* event_handler_arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data);

static esp_err_t root_get_handler(httpd_req_t* req);
static esp_err_t connect_get_handler(httpd_req_t* req);
static esp_err_t wildcard_get_handler(httpd_req_t* req);
static esp_err_t redirect_to_root(httpd_req_t* req);

// Start application tasks (Firebase + DHT11 + button)
static void
start_application_tasks()
{
    xTaskCreate(firebase_dht11_task, "DHT11_Firebase", 8192, NULL, 5, NULL);

    xTaskCreate(firebase_switch_stream_task, "FirebaseStream", 8192, NULL, 7, NULL);

    xTaskCreate(button_handler_task, "ButtonHandler", 4096, NULL, 10, NULL);
}

// Start the web server (captive portal)
static httpd_handle_t
start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_req_hdr_len = 2048;
    config.uri_match_fn = httpd_uri_match_wildcard;

    if (httpd_start(&server, &config) == ESP_OK)
    {
        // Register HTTP endpoints
        httpd_uri_t root_uri = {.uri = "/", .method = HTTP_GET, .handler = root_get_handler};
        httpd_register_uri_handler(server, &root_uri);

        httpd_uri_t connect_uri
            = {.uri = "/connect", .method = HTTP_GET, .handler = connect_get_handler};
        httpd_register_uri_handler(server, &connect_uri);

        httpd_uri_t gen_uri
            = {.uri = "/generate_204", .method = HTTP_GET, .handler = redirect_to_root};
        httpd_register_uri_handler(server, &gen_uri);

        httpd_uri_t hotspot_uri
            = {.uri = "/hotspot-detect.html", .method = HTTP_GET, .handler = redirect_to_root};
        httpd_register_uri_handler(server, &hotspot_uri);

        httpd_uri_t ncsi_uri
            = {.uri = "/ncsi.txt", .method = HTTP_GET, .handler = redirect_to_root};
        httpd_register_uri_handler(server, &ncsi_uri);

        httpd_uri_t wildcard_uri
            = {.uri = "/*", .method = HTTP_GET, .handler = wildcard_get_handler};
        httpd_register_uri_handler(server, &wildcard_uri);
    }
    return server;
}

// WiFi and IP event handler
static void
wifi_event_handler(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id,
                   void* event_data)
{
    static int retry_cnt = 0;
    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGE(TAG, "ESP32 Disconnected, retrying");
            retry_cnt++;
            if (retry_cnt < MAX_RETRY_NUM)
            {
                esp_wifi_connect();
            }
            else
            {
                ESP_LOGE(TAG, "Connection error, starting AP");
                ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
                wifi_config_t empty = {0};
                esp_wifi_set_config(WIFI_IF_STA, &empty);

                wifi_config_t ap_config = {0};
                strncpy((char*)ap_config.ap.ssid, AP_SSID, sizeof(ap_config.ap.ssid) - 1);
                ap_config.ap.ssid_len = strlen(AP_SSID);
                strncpy((char*)ap_config.ap.password, AP_PASS, sizeof(ap_config.ap.password) - 1);
                ap_config.ap.authmode = WIFI_AUTH_OPEN;
                ap_config.ap.max_connection = 1;

                ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
                ESP_ERROR_CHECK(esp_wifi_start());
                dns_server_start();
                start_webserver();
            }
            break;
        default:
            break;
        }
    }
    else if (event_base == IP_EVENT)
    {
        if (event_id == IP_EVENT_STA_GOT_IP)
        {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
            ESP_LOGI(TAG, "station ip :" IPSTR, IP2STR(&event->ip_info.ip));

            if (!tasks_started)
            {
                start_application_tasks();
                tasks_started = true;
            }
        }
    }
}

void
wifi_provisioning_start(void)
{
    ESP_LOGI(TAG, "Initializing WiFi stack for provisioning");
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

    // Check if wifi config is already saved
    wifi_config_t saved_config;
    bool is_provisioned = false;
    if (esp_wifi_get_config(WIFI_IF_STA, &saved_config) == ESP_OK
        && strlen((const char*)saved_config.sta.ssid) > 0)
    {
        is_provisioned = true;
    }

    if (is_provisioned)
    {
        ESP_LOGI(TAG, "Device already provisioned. Connecting to '%s'", saved_config.sta.ssid);
        saved_config.sta.listen_interval = MAX_LISTEN_INTERVAL;
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        dns_server_stop();
        ESP_ERROR_CHECK(esp_wifi_start());
    }
    else
    {
        ESP_LOGI(TAG, "Device not provisioned. Starting SoftAP and captive portal");
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

        wifi_config_t ap_config = {0};
        strncpy((char*)ap_config.ap.ssid, AP_SSID, sizeof(ap_config.ap.ssid) - 1);
        ap_config.ap.ssid_len = strlen(AP_SSID);
        strncpy((char*)ap_config.ap.password, AP_PASS, sizeof(ap_config.ap.password) - 1);
        ap_config.ap.authmode = WIFI_AUTH_OPEN;
        ap_config.ap.max_connection = 1;
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
        ESP_ERROR_CHECK(esp_wifi_start());

        dns_server_start();
        // start the webserver that serves the provisioning HTML
        start_webserver();
        ESP_LOGI(TAG, "Captive portal started at 192.168.4.1");
    }
}

static esp_err_t
root_get_handler(httpd_req_t* req)
{
    httpd_resp_send(req, html_form, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t
connect_get_handler(httpd_req_t* req)
{
    char ssid[32] = {0};
    char password[64] = {0};
    char buf[100];
    size_t buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1)
    {
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
        {
            if (httpd_query_key_value(buf, "ssid", ssid, sizeof(ssid)) != ESP_OK
                || httpd_query_key_value(buf, "password", password, sizeof(password)) != ESP_OK)
            {
                httpd_resp_send_404(req);
                return ESP_FAIL;
            }
        }
    }

    ESP_LOGI(TAG, "Otrzymano SSID: %s, Haslo: %s", ssid, password);

    wifi_config_t wifi_config = {};
    strcpy((char*)wifi_config.sta.ssid, ssid);
    strcpy((char*)wifi_config.sta.password, password);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_connect());

    httpd_resp_send(req, "<h1>Probuje polaczyc...</h1><p>Restart urzadzenia po sukcesie.</p>",
                    HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t
redirect_to_root(httpd_req_t* req)
{
    const char* location = "http://192.168.4.1/";
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", location);
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "Expires", "0");

    // Send a tiny HTML page with meta-refresh as a fallback for user agents
    const char* body = "<html><head>"
                       "<meta http-equiv=\"refresh\" content=\"0;url=http://192.168.4.1/\">"
                       "</head><body></body></html>";
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t
wildcard_get_handler(httpd_req_t* req)
{
    if (strcmp(req->uri, "/connect") == 0)
    {
        return httpd_resp_send_404(req);
    }
    httpd_resp_send(req, html_form, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}
