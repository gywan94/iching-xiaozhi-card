# 解析 xiaozhi-claw 根目录：优先环境变量 XIAOZHI_CLAW_ROOT，否则默认 D:/cursorproject/xiaozhi-4-21/XiaoZhi-Card
if(DEFINED ENV{XIAOZHI_CLAW_ROOT} AND NOT "$ENV{XIAOZHI_CLAW_ROOT}" STREQUAL "")
    file(TO_CMAKE_PATH "$ENV{XIAOZHI_CLAW_ROOT}" XIAOZHI_CLAW_ROOT)
else()
    get_filename_component(XIAOZHI_CLAW_ROOT "D:/cursorproject/xiaozhi-4-21/XiaoZhi-Card" ABSOLUTE)
endif()
