#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/spi_master.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "DEV_Config.h"
#include "EPD_2IN9.h"
#include "EPD_2IN9_V2.h"
#include "GUI_Paint.h"
#include "EAN-13.h"
#include "qrcodegen.h"

//EPD Version
#define EPD_VER             2

//WiFi
#define ESP_WIFI_SSID       "SSID"
#define ESP_WIFI_PASS       "PASS"
#define ESP_MAXIMUM_RETRY   10

//TCP Server
#define PORT                80
#define KEEPALIVE_IDLE      5
#define KEEPALIVE_INTERVAL  5
#define KEEPALIVE_COUNT     3

static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1

#define REBOOT_TIMEOUT      3600
#define QR_DEALY            4000

const char *TAG = "QR Barcode Test Stand";
static int s_retry_num = 0;
uint8_t *ScreenBuf;

tcpip_adapter_ip_info_t ip_info;
uint32_t ip_address;
char ip_str[16];
uint8_t new_message = 0;
char message[128];
uint8_t qr_count = 1;

char bar_code[13];
const char barcode_text[] =    "barcode ";
char stand_start[] =          "stand_start";
char stand_end[] =             "stand_end";


uint8_t stand_start_cmd = 0, 
        stand_end_cmd = 0,
        new_barcode = 0;

uint32_t time_to_off = REBOOT_TIMEOUT;


void delay(uint32_t ms) {
    vTaskDelay(ms / portTICK_RATE_MS);
}

void ESP_Restart(uint16_t sec)
{
  // Restart module
  for (int i = sec; i > 0; i--) {
    ESP_LOGI(TAG,"Restarting in %d seconds...", i);
    delay(1000);
  }
  ESP_LOGI(TAG,"Restarting now.");
  fflush(stdout);
  esp_restart();
}

static void printQr(const uint8_t qrcode[]) 
{
    int size = qrcodegen_getSize(qrcode);
    uint32_t box_size = 4;                                          // 3 ->  med: 4, high: 4
    uint32_t offset_x = (EPD_2IN9_HEIGHT - (size * box_size)) / 2;  // 8 -> med: 6, high: 0
    uint32_t offset_y = 10;                                         // 32 -> med: 6, high: 0
	int border = 0;                                                 // 2 -> 2
	for (int y = -border; y < size + border; y++) {
		for (int x = -border; x < size + border; x++) {
            uint32_t x1 = offset_x + x * box_size;
            uint32_t y1 = offset_y + y * box_size;
            uint32_t x2 = x1 + box_size;
            uint32_t y2 = y1 + box_size;
            Paint_DrawRectangle(x1, y1, x2, y2, qrcodegen_getModule(qrcode, x, y) ? BLACK : WHITE , DOT_PIXEL_1X1, DRAW_FILL_FULL);
		}
	}
}

static void epd_display_task(void *pvParameters) {

    char str[128];
    //EPD Init
    DEV_Module_Init();
    if(EPD_VER == 2) EPD_2IN9_V2_Init();
    else if(EPD_VER == 1) EPD_2IN9_Init(EPD_2IN9_FULL);

    uint16_t Imagesize = ((EPD_2IN9_V2_WIDTH % 8 == 0)? (EPD_2IN9_V2_WIDTH / 8 ): (EPD_2IN9_V2_WIDTH / 8 + 1)) * EPD_2IN9_V2_HEIGHT;
    if((ScreenBuf = (UBYTE *)malloc(Imagesize)) == NULL) {
        ESP_LOGE(TAG,"Failed to apply for screen buffer memory...");
        while(1);
    }
    Paint_NewImage(ScreenBuf, EPD_2IN9_V2_WIDTH, EPD_2IN9_V2_HEIGHT, 270, WHITE);
    Paint_Clear(WHITE);

    //Start message
    Paint_DrawString_EN(63, 52, "TEST STAND", &Font24, BLACK, WHITE);
    if(EPD_VER == 2)  EPD_2IN9_V2_Display(ScreenBuf);
    else if(EPD_VER == 1) EPD_2IN9_Display(ScreenBuf);
    delay(2000);

    time_to_off = REBOOT_TIMEOUT;

    //IP Show
    while(qr_count || new_message == 0) {
        if(qr_count) qr_count--;
        sprintf(ip_str, "%d.%d.%d.%d", IP2STR(&ip_info.ip));
        sprintf(str, "IP:%s:%d", ip_str, PORT);
        Paint_Clear(WHITE);
        Paint_DrawString_EN((uint16_t)((EPD_2IN9_HEIGHT - strlen(str) * Font16.Width) / 2), 110, str, &Font16, BLACK, WHITE);

        uint8_t qrcode[qrcodegen_BUFFER_LEN_MAX];
        uint8_t tempBuffer[qrcodegen_BUFFER_LEN_MAX];
        bool ok = qrcodegen_encodeText(ip_str, tempBuffer, qrcode, qrcodegen_Ecc_LOW,
            qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX, qrcodegen_Mask_0, true);
        if(ok) printQr(qrcode);

        if(EPD_VER == 2)  EPD_2IN9_V2_Display(ScreenBuf);
        else if(EPD_VER == 1) EPD_2IN9_Display(ScreenBuf);

        delay(QR_DEALY);
    }
    while(1) {

        if(new_message) {
            new_message = 0;
            ESP_LOGI(TAG,"New message [%d] - [%s]", strlen(message), message);
            time_to_off = REBOOT_TIMEOUT;

            if(strlen(message) == 21) {

                uint8_t bar_ok = 1;
                for(int i=0; i<22; i++) {
                if(i<8) { 
                    if(message[i] != barcode_text[i]) { 
                        bar_ok=0; 
                        break; 
                    }
                }
                else bar_code[i-8] = message[i];
                }
                if(bar_ok) {
                    Paint_Clear(WHITE);
                    if(EPD_VER == 2)  EPD_2IN9_V2_Display_Partial(ScreenBuf);
                    drawBarCode(20, 80, 2, bar_code);
                    sprintf(str, "%s",message);
                    Paint_DrawString_EN((uint16_t)((EPD_2IN9_HEIGHT - strlen(str) * Font16.Width) / 2), 110, str, &Font16, BLACK, WHITE);

                    if(EPD_VER == 2)  EPD_2IN9_V2_Display_Partial(ScreenBuf);
                    else if(EPD_VER == 1) EPD_2IN9_Display(ScreenBuf);
                }
            }

            else if(strlen(message) == 9) {
                uint8_t stand_end_ok = 1;
                for(int i = 0; i < 9; i++) {
                    if(message[i] != stand_end[i]) { stand_end_ok = 0; break; }
                }
                if(stand_end_ok) {
                    stand_start_cmd=0;
                    Paint_Clear(WHITE);
                    if(EPD_VER == 2)  EPD_2IN9_V2_Display_Partial(ScreenBuf);
                    sprintf(str, "Test Ended");
                    Paint_DrawString_EN((uint16_t)((EPD_2IN9_HEIGHT - strlen(str) * Font24.Width) / 2), 52, str, &Font24, BLACK, WHITE);
                    ESP_LOGI(TAG,"Test Ended");
                    if(EPD_VER == 2)  EPD_2IN9_V2_Display_Partial(ScreenBuf);
                    else if(EPD_VER == 1) EPD_2IN9_Display(ScreenBuf);
                    stand_end_cmd = 1;
                    delay(1000);
                    ESP_Restart(0);
                    //esp_wifi_disconnect();
                    //vTaskDelete(NULL);
                }
            }

            else {
                if(strlen(message) <= 26) {

                    Paint_Clear(WHITE);
                    if(EPD_VER == 2)  EPD_2IN9_V2_Display_Partial(ScreenBuf);
                    sprintf(str, "%s",message);
                    Paint_DrawString_EN((uint16_t)((EPD_2IN9_HEIGHT - strlen(str) * Font16.Width) / 2), 110, str, &Font16, BLACK, WHITE);

                    if(EPD_VER == 2)  EPD_2IN9_V2_Display_Partial(ScreenBuf);
                    else if(EPD_VER == 1) EPD_2IN9_Display(ScreenBuf);
                }
            }          
        }
        delay(100);
    }
    vTaskDelete(NULL);
}

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) esp_wifi_connect(); 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } 
        else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } 
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void) {
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = ESP_WIFI_SSID,
            .password = ESP_WIFI_PASS,
	        .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 ESP_WIFI_SSID, ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 ESP_WIFI_SSID, ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
}

static void do_retransmit(const int sock)
{
    int len;
    char rx_buffer[128];

    do {
        //memset(rx_buffer, 0, sizeof(rx_buffer));
        len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len < 0) {
            ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
        } else if (len == 0) {
            ESP_LOGW(TAG, "Connection closed");
        } else {
            rx_buffer[len] = 0; // Null-terminate whatever is received and treat it like a string
            ESP_LOGI(TAG, "Received %d bytes: %s", len, rx_buffer);

            if(len>2){  
                memset(message, 0, sizeof(message));
                memcpy(message, rx_buffer, len-2);
                new_message = 1;
            }
            int to_write = len;
            while (to_write > 0) {
                int written = send(sock, rx_buffer + (len - to_write), to_write, 0);
                if (written < 0) {
                    ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                }
                to_write -= written;
            }
        }
    } while (len > 0);
}

static void tcp_server_task(void *pvParameters)
{
    char addr_str[128];
    int addr_family = (int)pvParameters;
    int ip_protocol = 0;
    int keepAlive = 1;
    int keepIdle = KEEPALIVE_IDLE;
    int keepInterval = KEEPALIVE_INTERVAL;
    int keepCount = KEEPALIVE_COUNT;
    struct sockaddr_storage dest_addr;

    if (addr_family == AF_INET) {
        struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
        dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr_ip4->sin_family = AF_INET;
        dest_addr_ip4->sin_port = htons(PORT);
        ip_protocol = IPPROTO_IP;
    }
#ifdef CONFIG_EXAMPLE_IPV6
    else if (addr_family == AF_INET6) {
        struct sockaddr_in6 *dest_addr_ip6 = (struct sockaddr_in6 *)&dest_addr;
        bzero(&dest_addr_ip6->sin6_addr.un, sizeof(dest_addr_ip6->sin6_addr.un));
        dest_addr_ip6->sin6_family = AF_INET6;
        dest_addr_ip6->sin6_port = htons(PORT);
        ip_protocol = IPPROTO_IPV6;
    }
#endif

    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#if defined(CONFIG_EXAMPLE_IPV4) && defined(CONFIG_EXAMPLE_IPV6)
    // Note that by default IPV6 binds to both protocols, it is must be disabled
    // if both protocols used at the same time (used in CI)
    setsockopt(listen_sock, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt));
#endif

    ESP_LOGI(TAG, "Socket created");

    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        ESP_LOGE(TAG, "IPPROTO: %d", addr_family);
        goto CLEAN_UP;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", PORT);

    err = listen(listen_sock, 1);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        goto CLEAN_UP;
    }

    while (1) {
        if(stand_end_cmd) goto CLEAN_UP;
        ESP_LOGI(TAG, "Socket listening");

        struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
        socklen_t addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }

        // Set tcp keepalive option
        setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));
        // Convert ip address to string
        if (source_addr.ss_family == PF_INET) {
            inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
        }
#ifdef CONFIG_EXAMPLE_IPV6
        else if (source_addr.ss_family == PF_INET6) {
            inet6_ntoa_r(((struct sockaddr_in6 *)&source_addr)->sin6_addr, addr_str, sizeof(addr_str) - 1);
        }
#endif
        ESP_LOGI(TAG, "Socket accepted ip address: %s", addr_str);

        do_retransmit(sock);

        shutdown(sock, 0);
        close(sock);
    }

CLEAN_UP:
    close(listen_sock);
    vTaskDelete(NULL);
}

static void timer_task(void *pvParameters)
{
    time_to_off = REBOOT_TIMEOUT;
    while(1) {
        if(time_to_off) time_to_off--; 
        else {
            ESP_LOGI(TAG,"TIMEOUT!");
            ESP_Restart(0);
        }
        delay(1000);
    }
    vTaskDelete(NULL);
}

void app_main() {
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    ESP_ERROR_CHECK(esp_netif_init());
    wifi_init_sta();

    memset(&ip_info, 0, sizeof(ip_info));
    tcpip_adapter_get_ip_info(ESP_IF_WIFI_STA , &ip_info);
    ip_address = ip_info.ip.addr;
    if(ip_address == 0) ESP_Restart(0);
    xTaskCreate(timer_task, "timer", 1024, NULL, 20, NULL);
    xTaskCreate(epd_display_task, "epd_display", 16384, NULL, 10, NULL);
    xTaskCreate(tcp_server_task, "tcp_server", 4096, (void*)AF_INET, 5, NULL);
}