idf_component_register(SRCS "cnip_http.c"
                            "cnip_sha1.c"
                            "http_custom.c"
                            "mfs.c"
                            "networking.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_wifi cnip spi_flash fatfs vfs wear_levelling
                    PRIV_REQUIRES esptool_py)

