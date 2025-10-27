#include "hardware.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "firebase.h"

#define RELAY_GPIO_PIN 22
#define RELAY_ON 1
#define RELAY_OFF 0
#define RELAY_IMPULSE_TIME_MS 500

#define DEBOUNCE_TIME_MS 3000
#define BUTTON_GPIO_PIN 17

static QueueHandle_t gpio_evt_queue = NULL;
static bool relay_state = RELAY_OFF;

// Interrupt service routine for button press
static void IRAM_ATTR
button_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

void
relay_init(void)
{
    gpio_reset_pin(RELAY_GPIO_PIN);
    gpio_set_direction(RELAY_GPIO_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(RELAY_GPIO_PIN, RELAY_OFF);
}

void
set_relay_state(const char* json_payload)
{
    if (json_payload == NULL)
        return;

    if (strstr(json_payload, "true") != NULL)
    {
        if (!relay_state)
        {
            gpio_set_level(RELAY_GPIO_PIN, RELAY_ON);
            vTaskDelay(pdMS_TO_TICKS(RELAY_IMPULSE_TIME_MS));
            gpio_set_level(RELAY_GPIO_PIN, RELAY_OFF);
        }
        relay_state = RELAY_ON;
        ESP_LOGI("RELAY", "RELAY SET HIGH.");
    }
    else if (strstr(json_payload, "false") != NULL)
    {
        if (relay_state)
        {
            gpio_set_level(RELAY_GPIO_PIN, RELAY_ON);
            vTaskDelay(pdMS_TO_TICKS(RELAY_IMPULSE_TIME_MS));
            gpio_set_level(RELAY_GPIO_PIN, RELAY_OFF);
        }
        relay_state = RELAY_OFF;
        ESP_LOGI("RELAY", "RELAY SET LOW.");
    }
    else
    {
        ESP_LOGE("RELAY", "Received unrecognized payload: %s", json_payload);
    }
}

void
pc_switch_init()
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE, // Trigger on falling edge
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BUTTON_GPIO_PIN),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE, // Enable pull-up resistor
    };

    gpio_config(&io_conf);

    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    gpio_install_isr_service(0);

    gpio_isr_handler_add(BUTTON_GPIO_PIN, button_isr_handler, (void*)BUTTON_GPIO_PIN);
}

// Task to handle button presses with debounce
void
button_handler_task(void* pvParameters)
{
    uint32_t io_num;
    uint64_t last_interrupt_time = 0;

    for (;;)
    {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY))
        {

            uint64_t current_time = esp_timer_get_time() / 1000; // Time in ms
            if (current_time - last_interrupt_time < DEBOUNCE_TIME_MS)
            {
                continue; // Ignore presses within debounce period
            }

            ESP_LOGI("BUTTON_TASK", "Button on GPIO %d pressed! Time: %llums", io_num,
                     current_time);

            // Toggle relay state and send to Firebase
            relay_state = !relay_state;
            firebase_put("CONTROLS/pc_switch", relay_state);

            last_interrupt_time = current_time;
        }
    }
}
