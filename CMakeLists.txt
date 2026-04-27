cmake_minimum_required(VERSION 3.16)
include(${CMAKE_SOURCE_DIR}/cmake/xiaozhi_claw_root.cmake)
if(NOT EXISTS "${XIAOZHI_CLAW_ROOT}/components/esp_epd_gdey027t91/CMakeLists.txt")
    message(FATAL_ERROR
        "未找到 xiaozhi-claw 依赖 (EPD、LVGL port、字库、电量计等): ${XIAOZHI_CLAW_ROOT}\n"
        "请把 XiaoZhi-Card 放在 D:/cursorproject/xiaozhi-4-21/XiaoZhi-Card，\n"
        "或设置环境变量 XIAOZHI_CLAW_ROOT 指向含 components/ 的目录。")
endif()
set(EXTRA_COMPONENT_DIRS
    "${XIAOZHI_CLAW_ROOT}/components/esp_epd_gdey027t91"
    "${XIAOZHI_CLAW_ROOT}/components/esp_lvgl_port"
)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(xiaozhi_dayan_app)
