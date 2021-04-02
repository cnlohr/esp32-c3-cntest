idf_component_register(SRCS "cnip_core.c"
                            "cnip_dhcp.c"
                            "cnip_hal.c"
                            "cnip_tcp.c"
                            "tcpip_adapter.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_wifi
                    PRIV_REQUIRES esptool_py)

