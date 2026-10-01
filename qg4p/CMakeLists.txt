# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
# ---------------------------------------------------------------------------
#  QG4P - QuickGraphics 4 Pico
#
#  Two libraries:
#    qg4p          the graphics library (screens, drawing, text, images,
#                  and the optional photosensitivity-safer flash, qg_flash.h)
#    qg4p_assets   the asset pack reader (optional; independent of qg4p)
#
#  In your project's CMakeLists.txt:
#      add_subdirectory(qg4p)
#      target_link_libraries(my_app pico_stdlib qg4p)            # graphics
#      target_link_libraries(my_app pico_stdlib qg4p qg4p_assets) # + assets
# ---------------------------------------------------------------------------
add_library(qg4p STATIC
    qg_screen.c
    qg_palette.c
    qg_draw.c
    qg_draw_pct.c
    qg_block.c
    qg_text.c
    qg_image.c
    qg_flash.c
    fonts/qg_font_mono_12.c
    fonts/qg_font_sans_16.c
    fonts/qg_font_sans_bold_24.c
    backend/qg_backend_direct.c
    backend/qg_backend_buf8.c
    drivers/qg_driver.c
    drivers/qg_drv_st7789.c
    drivers/qg_drv_ili9341.c
    drivers/qg_drv_st7796.c
    hal/qg_hal.c
)
# Headers are included relative to this folder, e.g. "hal/qg_hal.h".
target_include_directories(qg4p PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(qg4p PUBLIC
    pico_stdlib
    hardware_spi
    hardware_dma
    hardware_pwm
    hardware_clocks
)

add_library(qg4p_assets STATIC assets/qg_asset.c)
target_include_directories(qg4p_assets PUBLIC ${CMAKE_CURRENT_LIST_DIR}/assets)
target_link_libraries(qg4p_assets PUBLIC pico_stdlib)
