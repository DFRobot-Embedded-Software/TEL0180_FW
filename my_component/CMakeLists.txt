idf_component_register(SRCS "./src/rgb.c"
                            "./src/i2c_slave.c"
                            "./src/spi_ffs.c"
                            "./src/group.c"
                            "./src/i2c_master.c"
                            "./src/wifi.c"
                            "./src/mqtt.c"
                            "./src/http.c"

                        INCLUDE_DIRS "."
                                     "./inc"
                        REQUIRES spiffs
                                 esp_wifi
                                 hal
                                 wpa_supplicant
                                 nvs_flash
                                 mqtt
                                 esp_pm
                                 esp_http_server
                                 )