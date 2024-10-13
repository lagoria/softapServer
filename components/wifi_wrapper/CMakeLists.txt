idf_component_register(SRCS "wifi_wrapper.c"
                    INCLUDE_DIRS "."
                    REQUIRES
                        esp_wifi
                        nvs_flash
                        )
