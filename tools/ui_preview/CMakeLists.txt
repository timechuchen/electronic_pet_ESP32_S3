cmake_minimum_required(VERSION 3.16)
project(pet_ui_preview C CXX)
set(CMAKE_C_STANDARD 11)
set(PET_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_LIST_DIR}/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
add_subdirectory("${PET_ROOT}/managed_components/lvgl__lvgl" lvgl)
find_package(JPEG REQUIRED)
add_executable(pet_ui_preview preview.c "${PET_ROOT}/main/pet_ui.c"
    "${PET_ROOT}/main/pet_font_zh_16.c" "${PET_ROOT}/main/pet_font_zh_20.c"
    "${PET_ROOT}/main/pet_font_zh_28.c")
target_include_directories(pet_ui_preview PRIVATE "${PET_ROOT}/main")
target_link_libraries(pet_ui_preview PRIVATE lvgl::lvgl JPEG::JPEG)
target_compile_options(pet_ui_preview PRIVATE -Wall -Wextra -Werror -UNDEBUG)
enable_testing()
add_test(NAME pet_ui_interactions COMMAND pet_ui_preview
    "${PET_ROOT}/assets/pet_frames.bin" "${CMAKE_CURRENT_BINARY_DIR}")
