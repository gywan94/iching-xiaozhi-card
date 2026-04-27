get_filename_component(_ICHD_PROJ_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
include(${_ICHD_PROJ_DIR}/cmake/xiaozhi_claw_root.cmake)
idf_component_register(
    SRCS
        "main.cc"
        "board_hal.cc"
        "dayan_app.cc"
        "dayan_ui.cc"
        "dayan_engine.cc"
        "dayan_data.cc"
        "font_iching_patch_16_1.c"
        "${XIAOZHI_CLAW_ROOT}/components/guage_bq27220/bq27220.cpp"
        "${XIAOZHI_CLAW_ROOT}/components/charger_aw32001/aw32001.cpp"
        "${XIAOZHI_CLAW_ROOT}/components/xiaozhi-fonts/src/font_puhui_16_1.c"
        "${XIAOZHI_CLAW_ROOT}/components/xiaozhi-fonts/src/font_awesome_16_4.c"
    INCLUDE_DIRS "."
        "${XIAOZHI_CLAW_ROOT}/components/guage_bq27220/include"
        "${XIAOZHI_CLAW_ROOT}/components/charger_aw32001/include"
        "${XIAOZHI_CLAW_ROOT}/components/xiaozhi-fonts/include"
)
