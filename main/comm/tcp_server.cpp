#include "tcp_server.h"
#include "socket_wrapper.h"
#include "wifi_wrapper.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <lwip/sockets.h>
#include <esp_log.h>
#include <cstring>
#include <atomic>


namespace TcpServer {

constexpr static const char TAG[] = "tcp_server";
static SocketWrapper::Server* _tcp_server = nullptr;
static RecvCallback _recv_cb = nullptr;
/* 受连接的TCP客户端信息链表头 */
static ClientInfo* _client_info_head = nullptr;
static std::atomic_int	_source_sock = -1;

static void add_tcp_client_list_node(int socket) 
{  
    ClientInfo *new_client = (ClientInfo *)malloc(sizeof(ClientInfo));  
    if (new_client == NULL) return;
    if (_client_info_head == NULL) {
        _client_info_head = new_client;
    } else {
        ClientInfo *current;
        current = _client_info_head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = new_client;
    }

    // 设置节点信息
    memset(new_client, 0, sizeof(ClientInfo));
    new_client->socket = socket;
    new_client->next = NULL;
}

static void delete_tcp_client_list_node(int socket)
{
    if (_client_info_head == NULL) return;
    ClientInfo *current, *prev;
    current = _client_info_head;
    // 判断是否为第一个节点
    if (current != NULL && current->socket == socket) {
        _client_info_head = current->next;
        free(current);
        return;
    }
    while (current->next != NULL) {
        prev = current;
        current = current->next;
        if (current != NULL && current->socket == socket) {
            prev->next = current->next;
            free(current);
            break;
        }
    }
}

static void tcp_recv_task(void *pvParameters) {
    int *pragma = (int *)pvParameters;
    int fd = *pragma;
    SocketWrapper::Socket socket(fd);
    ESP_LOGI(TAG, "client_sock = %d", fd);
    {   // 获取客户端IP地址(作用域限定)
        struct sockaddr client_addr;
        socklen_t client_addr_len = sizeof(struct sockaddr);
        int ret = getpeername(fd, &client_addr, &client_addr_len);
        if (ret == 0) {
            struct sockaddr_in *client_addr_in = (struct sockaddr_in *)&client_addr;
            char *client_ip = inet_ntoa(client_addr_in->sin_addr);
            char client_port = ntohs(client_addr_in->sin_port);
            uint8_t client_id = (uint8_t ) (client_addr_in->sin_addr.s_addr >> 24);
            ESP_LOGI(TAG, "Client IP:%s,Port:%d", client_ip, client_port);
            // 保存客户端IP地址,端口号
            for (ClientInfo *list = _client_info_head; list; list = list->next) {
                if (list->socket == fd) {
                    strcpy(list->ip, client_ip);
                    list->port = client_port;
                    // ip地址的机器码作id
                    list->id = client_id;
                    break;
                }
            }
        }
    }

    /* allocation sock date buffer */
    uint8_t* rx_buf = (uint8_t *)malloc(SOCK_BUF_SIZE + 8);
    if (rx_buf == NULL) {
        ESP_LOGE(TAG, "sock buffer malloc failed");
        goto over;
    }

    while(1) {
        int recv_len = socket.recv(rx_buf, SOCK_BUF_SIZE + 8);
        if (recv_len < 0) {
            // Error occurred within this client's socket -> close and mark invalid
            ESP_LOGW(TAG, "[sock=%d]: recv() returned %d -> closing the socket", fd, recv_len);
            goto over;
        } else {
            if (_recv_cb != NULL) {
                _source_sock = fd;
                _recv_cb(fd, {rx_buf, (uint32_t )recv_len});          // 回调函数
            }
        }

    }

over:
    /* colse... */
    delete_tcp_client_list_node(fd);
    vTaskDelete(NULL);
}

static void tcp_listen_task(void *pvParameters) {
    // Main loop for accepting new connections and serving all connected clients
    while (1) {
        // Find a free socket
        uint8_t client_count = 0;
        for (ClientInfo *list = _client_info_head; list; list = list->next) {
            client_count++;
        }

        // We accept a new connection only if we have a free socket
        if (client_count < 6) {
            // Try to accept a new connections
            int sock = _tcp_server->accept();
            if (sock < 0) {
                ESP_LOGE(TAG, "Unable to accept connection.");
                break;
            } else {
                // add client infor list node
                add_tcp_client_list_node(sock);

                if (xTaskCreate(tcp_recv_task, "tcp_recv_task", 5 * 1024, (void *)&sock, 10, NULL) != pdPASS) {
                    ESP_LOGE(TAG, "tcp_recv_task create failed");
                }
            
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(1000));
            ESP_LOGW(TAG, "too many TCP connect");
        }
    }
}


void registerRecvCallback(RecvCallback cb) {
    _recv_cb = cb;
}

ClientInfo* getClientsInfo() {
    return _client_info_head;
}

int getSourceSock() {
    return _source_sock;
}

int init(uint16_t port) {
    int res;
    _tcp_server = new SocketWrapper::Server(SocketWrapper::Protocol::TCP);
    if (_tcp_server == nullptr) {
        ESP_LOGE(TAG, "socket error.");
        return -1;
    }

    res = _tcp_server->init(port);
    if (res < 0) {
        ESP_LOGE(TAG, "init failed.");
        return res;
    }

    xTaskCreate(tcp_listen_task, "tcp_listen_task", 6 * 1024, nullptr, 12, nullptr);

    return res;
}


}
