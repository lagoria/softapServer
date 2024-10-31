#include "shell_wrapper.h"
#include "cmds.h"
#include "nvs_wrapper.h"
#include "wifi_wrapper.h"
#include "tcp_server.h"
#include "tcp_data_handle.h"
#include "app_config.h"
#include "gui.h"

#include "esp_log.h"


extern "C" void app_main(void) {
    // Initialize NVS
    Wrapper::NVS::init("nvs");

    Wrapper::WiFi::netif_init();
    Wrapper::WiFi::Apsta::init(AppCfg::SOFTAP_SSID, AppCfg::SOFTAP_PAWD);

    Wrapper::Shell::registerCallback(cmds::call);

    // 创建TCP服务器
    TcpServer::init(AppCfg::SERVER_PORT);
    TcpDataHandle::init();
    TcpServer::registerRecvCallback(TcpDataHandle::response);

    gui::init();
}


