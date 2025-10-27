#include "dns_server.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include <stdio.h>
#include <string.h>

static const char* TAG = "dns_server";
static TaskHandle_t dns_task_handle = NULL;

static const char captive_ip[] = {192, 168, 4, 1};

// Minimal DNS packet helpers
struct dns_header
{
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} __attribute__((packed));

static int
parse_qname(const uint8_t* buf, int offset, char* out, int outlen)
{
    int i = offset;
    int pos = 0;
    while (buf[i] != 0 && i < 512)
    {
        uint8_t len = buf[i++];
        if (len + pos + 1 >= outlen)
            return -1;

        for (int j = 0; j < len; j++)
        {
            out[pos++] = buf[i++];
        }
        out[pos++] = '.';
    }

    if (pos == 0)
    {
        out[0] = '\0';
    }
    else
    {
        out[pos - 1] = '\0'; // remove trailing dot
    }

    return i + 1; // position after the trailing zero
}

static void
dns_task(void* arg)
{
    (void)arg;

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0)
    {
        ESP_LOGE(TAG, "Failed to create socket");
        vTaskDelete(NULL);
        return;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(53);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        ESP_LOGE(TAG, "Failed to bind socket");
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "DNS server started on UDP/53");

    uint8_t buf[512];
    while (1)
    {
        struct sockaddr_in src_addr;
        socklen_t socklen = sizeof(src_addr);
        int len = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&src_addr, &socklen);
        if (len <= 0)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if (len < (int)sizeof(struct dns_header))
            continue;
        struct dns_header* hdr = (struct dns_header*)buf;
        uint16_t qdcount = ntohs(hdr->qdcount);
        if (qdcount == 0)
            continue;

        // Parse question section
        int pos = sizeof(struct dns_header);
        char qname[256];
        int next = parse_qname(buf, pos, qname, sizeof(qname));
        if (next < 0)
            continue;
        pos = next;
        if (pos + 4 > len)
            continue; // type(2) + class(2)

        // Prepare response in the same buffer
        hdr->flags = htons(0x8180); // Standard response, no error
        hdr->ancount = htons(1);
        hdr->nscount = 0;
        hdr->arcount = 0;

        // Move write pointer to end of question
        int wpos = pos + 4;

        // Write answer: name as pointer to question (0xc00c)
        if (wpos + 16 > (int)sizeof(buf))
            continue;
        buf[wpos++] = 0xc0;
        buf[wpos++] = 0x0c;
        buf[wpos++] = 0x00; // TYPE A
        buf[wpos++] = 0x01;
        buf[wpos++] = 0x00; // CLASS IN
        buf[wpos++] = 0x01;
        buf[wpos++] = 0x00;
        buf[wpos++] = 0x00;
        buf[wpos++] = 0x00;
        buf[wpos++] = 0x3c; // TTL 60s
        buf[wpos++] = 0x00;
        buf[wpos++] = 0x04; // RDLENGTH 4

        // RDATA (IP address)
        buf[wpos++] = (uint8_t)captive_ip[0];
        buf[wpos++] = (uint8_t)captive_ip[1];
        buf[wpos++] = (uint8_t)captive_ip[2];
        buf[wpos++] = (uint8_t)captive_ip[3];

        int resp_len = wpos;
        sendto(sock, buf, resp_len, 0, (struct sockaddr*)&src_addr, socklen);
    }

    close(sock);
    vTaskDelete(NULL);
}

void
dns_server_start(void)
{
    if (dns_task_handle == NULL)
    {
        xTaskCreate(dns_task, "dns_task", 4096, NULL, 5, &dns_task_handle);
    }
}

void
dns_server_stop(void)
{
    if (dns_task_handle)
    {
        vTaskDelete(dns_task_handle);
        dns_task_handle = NULL;
    }
}
