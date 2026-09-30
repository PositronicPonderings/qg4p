# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
# ---------------------------------------------------------------------------
#  Examples: each builds as its own program, build/examples/qg4p_<name>.uf2.
#  They share board.c/board.h: set your wiring there once.
# ---------------------------------------------------------------------------
function(qg4p_example name)
    cmake_parse_arguments(E "ART;ASSETS" "" "" ${ARGN})
    add_executable(qg4p_${name} ${name}.c board.c)
    target_link_libraries(qg4p_${name} pico_stdlib qg4p)
    if (E_ART)
        target_sources(qg4p_${name} PRIVATE example_art.c)
    endif()
    if (E_ASSETS)
        target_link_libraries(qg4p_${name} qg4p_assets)
    endif()
    pico_enable_stdio_usb(qg4p_${name} 1)
    pico_enable_stdio_uart(qg4p_${name} 0)
    pico_add_extra_outputs(qg4p_${name})
endfunction()

qg4p_example(hello)                     #  1  the smallest program
qg4p_example(shapes)                    #  2  every primitive
qg4p_example(text)                      #  3  fonts, markup, wrap, tabs
qg4p_example(layout)                    #  4  percentages and views
qg4p_example(images ART)                #  5  images, transparency, scaling
qg4p_example(two_screens)               #  6  two screens, one bus
qg4p_example(asset_pack ASSETS)         #  7  loading by name from flash
qg4p_example(animation_direct)          #  8  motion without a framebuffer
qg4p_example(framebuffer)               #  9  flicker-free with BUF8
qg4p_example(palette_effects ART)       # 10  palette animation
qg4p_example(paint)                     # 11  PAINT and POINT
qg4p_example(sprites ART)               # 12  GET/PUT sprites
qg4p_example(dashboard)                 # 13  gauges and readouts
qg4p_example(dice_roller)               # 14  the game one
qg4p_example(colour_check)              # 15  colour test pattern
qg4p_example(calibrate)                 # 16  colour calibration over USB serial
