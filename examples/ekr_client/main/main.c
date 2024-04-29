/* WiFi station Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#ifndef _WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#else
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
#include <usrsctp.h>
#include "programs_helper.h"

#define MAX_PACKET_SIZE (1<<16)
#define BUFFER_SIZE 80
#define DISCARD_PPID 39



/* The examples use WiFi configuration that you can set via project configuration menu

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY

#if CONFIG_ESP_WPA3_SAE_PWE_HUNT_AND_PECK
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HUNT_AND_PECK
#define EXAMPLE_H2E_IDENTIFIER ""
#elif CONFIG_ESP_WPA3_SAE_PWE_HASH_TO_ELEMENT
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HASH_TO_ELEMENT
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#elif CONFIG_ESP_WPA3_SAE_PWE_BOTH
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_BOTH
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#endif
#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "wifi station";

static int s_retry_num = 0;

static bool volatile g_handle_packets_run;

int done = 0;
char *buf;

#ifdef _WIN32
static DWORD WINAPI
#else
static void *
#endif
handle_packets(void *arg)
{
#ifdef _WIN32
    SOCKET *fdp;
#else
    int *fdp;
#endif
    char *dump_buf;
    ssize_t length;
    buf = malloc(MAX_PACKET_SIZE);

#ifdef _WIN32
    fdp = (SOCKET *)arg;
#else
    fdp = (int *)arg;
#endif
    while (g_handle_packets_run) {
#if defined(__NetBSD__)
        pthread_testcancel();
#endif
        length = recv(*fdp, buf, MAX_PACKET_SIZE, 0);
        if (length > 0) {
            if ((dump_buf = usrsctp_dumppacket(buf, (size_t)length, SCTP_DUMP_INBOUND)) != NULL) {
                fprintf(stderr, "%s", dump_buf);
                usrsctp_freedumpbuffer(dump_buf);
            }
            usrsctp_conninput(fdp, buf, (size_t)length, 0);
        }
    }
#ifdef _WIN32
    return 0;
#else
    return (NULL);
#endif
}

static int
conn_output(void *addr, void *buf, size_t length, uint8_t tos, uint8_t set_df)
{
    char *dump_buf;
#ifdef _WIN32
    SOCKET *fdp;
#else
    int *fdp;
#endif

#ifdef _WIN32
    fdp = (SOCKET *)addr;
#else
    fdp = (int *)addr;
#endif
    if ((dump_buf = usrsctp_dumppacket(buf, length, SCTP_DUMP_OUTBOUND)) != NULL) {
        fprintf(stderr, "%s", dump_buf);
        usrsctp_freedumpbuffer(dump_buf);
    }
#ifdef _WIN32
    if (send(*fdp, buf, (int)length, 0) == SOCKET_ERROR) {
        return (WSAGetLastError());
#else
    if (send(*fdp, buf, length, 0) < 0) {
        return (errno);
#endif
    } else {
        return (0);
    }
}

static int
receive_cb(struct socket *sock, union sctp_sockstore addr, void *data,
           size_t datalen, struct sctp_rcvinfo rcv, int flags, void *ulp_info)
{

    if (data) {
        if (flags & MSG_NOTIFICATION) {
            printf("Notification of length %d received.\n", (int)datalen);
        } else {
            printf("Msg of length %d received via %p:%u on stream %u with SSN %u and TSN %lu, PPID %lu, context %lu.\n",
                   (int)datalen,
                   addr.sconn.sconn_addr,
                   ntohs(addr.sconn.sconn_port),
                   rcv.rcv_sid,
                   rcv.rcv_ssn,
                   rcv.rcv_tsn,
                   (uint32_t)ntohl(rcv.rcv_ppid),
                   rcv.rcv_context);
        }
        free(data);
    } else {
        usrsctp_deregister_address(ulp_info);
        usrsctp_close(sock);
    }
    return (1);
}

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (pasword len => 8).
             * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
             * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
             * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
             */
            .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
            .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
            .sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

void app_main(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();

    int argc = 5;
    char *argv[] = {"ekr_client", "192.168.1.165", "22222", "192.168.1.117", "11111"};

    struct sockaddr_in sin;
    struct sockaddr_conn sconn;
#ifdef _WIN32
    SOCKET fd;
#else
    int fd, rc;
#endif
    struct socket *s;
#ifdef _WIN32
    HANDLE tid;
#else
    pthread_t tid;
#endif
    struct sctp_sndinfo sndinfo;
    char buffer[BUFFER_SIZE];
#ifdef _WIN32
    WSADATA wsaData;
#endif

    if (argc < 4) {
        fprintf(stderr, "error: this program requires 4 arguments!\n");
        exit(EXIT_FAILURE);
    }

#ifdef _WIN32
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        exit (EXIT_FAILURE);
    }
#endif
    usrsctp_init(0, conn_output, debug_printf_stack);
    /* set up a connected UDP socket */
#ifdef _WIN32
    if ((fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET) {
        fprintf(stderr, "socket() failed with error: %d\n", WSAGetLastError());
        exit(EXIT_FAILURE);
    }
#else
    if ((fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
#endif
    memset(&sin, 0, sizeof(struct sockaddr_in));
    sin.sin_family = AF_INET;
#ifdef HAVE_SIN_LEN
    sin.sin_len = sizeof(struct sockaddr_in);
#endif
    sin.sin_port = htons(atoi(argv[2]));
    if (!inet_pton(AF_INET, argv[1], &sin.sin_addr.s_addr)){
        fprintf(stderr, "error: invalid address\n");
        exit(EXIT_FAILURE);
    }
#ifdef _WIN32
    if (bind(fd, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)) == SOCKET_ERROR) {
        fprintf(stderr, "bind() failed with error: %d\n", WSAGetLastError());
        exit(EXIT_FAILURE);
    }
#else
    if (bind(fd, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }
#endif
    memset(&sin, 0, sizeof(struct sockaddr_in));
    sin.sin_family = AF_INET;
#ifdef HAVE_SIN_LEN
    sin.sin_len = sizeof(struct sockaddr_in);
#endif
    sin.sin_port = htons(atoi(argv[4]));
    if (!inet_pton(AF_INET, argv[3], &sin.sin_addr.s_addr)){
        printf("error: invalid address\n");
        exit(EXIT_FAILURE);
    }
#ifdef _WIN32
    if (connect(fd, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)) == SOCKET_ERROR) {
        fprintf(stderr, "connect() failed with error: %d\n", WSAGetLastError());
        exit(EXIT_FAILURE);
    }
#else
    if (connect(fd, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)) < 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }
#endif
#ifdef SCTP_DEBUG
    usrsctp_sysctl_set_sctp_debug_on(SCTP_DEBUG_NONE);
#endif
    usrsctp_sysctl_set_sctp_ecn_enable(0);
    usrsctp_register_address((void *)&fd);
    g_handle_packets_run = true;
#ifdef _WIN32
    if ((tid = CreateThread(NULL, 0, &handle_packets, (void *)&fd, 0, NULL)) == NULL) {
        fprintf(stderr, "CreateThread() failed with error: %lu\n", GetLastError());
        exit(EXIT_FAILURE);
    }
#else
    if ((rc = pthread_create(&tid, NULL, &handle_packets, (void *)&fd)) != 0) {
        fprintf(stderr, "pthread_create: %s\n", strerror(rc));
        exit(EXIT_FAILURE);
    }
#endif
    if ((s = usrsctp_socket(AF_CONN, SOCK_STREAM, IPPROTO_SCTP, receive_cb, NULL, 0, &fd)) == NULL) {
        perror("usrsctp_socket");
    }

    memset(&sconn, 0, sizeof(struct sockaddr_conn));
    sconn.sconn_family = AF_CONN;
#ifdef HAVE_SCONN_LEN
    sconn.sconn_len = sizeof(struct sockaddr_conn);
#endif
    sconn.sconn_port = htons(0);
    sconn.sconn_addr = NULL;
    if (usrsctp_bind(s, (struct sockaddr *)&sconn, sizeof(struct sockaddr_conn)) < 0) {
        perror("usrsctp_bind");
    }

    memset(&sconn, 0, sizeof(struct sockaddr_conn));
    sconn.sconn_family = AF_CONN;
#ifdef HAVE_SCONN_LEN
    sconn.sconn_len = sizeof(struct sockaddr_conn);
#endif
    sconn.sconn_port = htons(5001);
    sconn.sconn_addr = &fd;
    if (usrsctp_connect(s, (struct sockaddr *)&sconn, sizeof(struct sockaddr_conn)) < 0) {
        perror("usrsctp_connect");
    }
    memset(buffer, 'A', BUFFER_SIZE);
    sndinfo.snd_sid = 1;
    sndinfo.snd_flags = 0;
    sndinfo.snd_ppid = htonl(DISCARD_PPID);
    sndinfo.snd_context = 0;
    sndinfo.snd_assoc_id = 0;
    if (usrsctp_sendv(s, buffer, BUFFER_SIZE, NULL, 0, (void *)&sndinfo,
                      (socklen_t)sizeof(struct sctp_sndinfo), SCTP_SENDV_SNDINFO, 0) < 0) {
        perror("usrsctp_sendv");
    }

    usrsctp_shutdown(s, SHUT_WR);
    while (usrsctp_finish() != 0) {
#ifdef _WIN32
        Sleep(1000);
#else
        vTaskDelay(1000 / portTICK_PERIOD_MS);
#endif
    }
#ifdef _WIN32
    TerminateThread(tid, 0);
    WaitForSingleObject(tid, INFINITE);
    if (closesocket(fd) == SOCKET_ERROR) {
        fprintf(stderr, "closesocket() failed with error: %d\n", WSAGetLastError());
    }
    WSACleanup();
#else
    if (close(fd) < 0) {
        perror("close");
    }
    g_handle_packets_run = false;
    pthread_join(tid, NULL);
    printf("exit!\n");
#endif
}
