#include "shell.h"
#include "cmds.h"
#include "nvs_wrapper.h"
#include "wifi_wrapper.h"
#include "firmware_wrapper.h"
#include "tcp_server.h"
#include "tcp_data_handle.h"
#include "app_config.h"
#include "gui.h"

#include "esp_log.h"


static const char *TAG = "app_main";


extern "C" void app_main(void) {
    ESP_LOGI(TAG, "%s", Firmware::info().c_str());

    // Initialize NVS
    NvsWrapper::init("nvs");

    WifiWrapper::netif_init();
    WifiWrapper::Apsta::init(CONFIG_SOFTAP_SSID, CONFIG_SOFTAP_PAS);

    Shell::registerCallback(cmds::call);

    // 创建TCP服务器
    TcpServer::init(TCP_SERVER_PORT);
    TcpDataHandle::init();
    TcpServer::registerRecvCallback(TcpDataHandle::response);

    gui::init();
}


