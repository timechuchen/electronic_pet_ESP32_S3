idf_component_register(SRCS "main.c" "pet_font_zh_20.c" "pet_font_zh_28.c" INCLUDE_DIRS "."
    REQUIRES esp_timer esp_driver_usb_serial_jtag esp_rom espressif__esp_new_jpeg)
target_add_binary_data(${COMPONENT_LIB} "${CMAKE_CURRENT_LIST_DIR}/../assets/pet_frames.bin" BINARY)
