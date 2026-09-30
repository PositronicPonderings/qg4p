# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
# ---------------------------------------------------------------------------
#  Hardware test programs, one per development milestone.
#
#  Every program here builds as its own target, so a single build produces
#  all of them: build/tests/hardware/qg4p_m0.uf2 ... qg4p_m8.uf2. To try a
#  different one, flash a different .uf2; no CMake editing needed.
#
#  Wiring and board choice: demo_setup.c (M2 onward) and docs/WIRING.md.
# ---------------------------------------------------------------------------

# qg4p_test(name source... [ASSETS]): one test program.
function(qg4p_test name)
    cmake_parse_arguments(T "ASSETS" "" "" ${ARGN})
    add_executable(${name} ${T_UNPARSED_ARGUMENTS})
    target_link_libraries(${name} pico_stdlib qg4p)
    if (T_ASSETS)
        target_link_libraries(${name} qg4p_assets)
    endif()
    pico_enable_stdio_usb(${name} 1)          # printf() goes to USB serial
    pico_enable_stdio_uart(${name} 0)
    pico_add_extra_outputs(${name})           # makes the .uf2
endfunction()

qg4p_test(qg4p_m0 m0_demo.c)                                      # one ST7789
qg4p_test(qg4p_m1 m1_demo.c)                                      # two screens
qg4p_test(qg4p_m2 m2_demo.c demo_setup.c)                         # shapes
qg4p_test(qg4p_m3 m3_demo.c demo_setup.c)                         # percentages
qg4p_test(qg4p_m4 m4_demo.c demo_setup.c)                         # fonts
qg4p_test(qg4p_m5 m5_demo.c demo_setup.c)                         # markup, scroll
qg4p_test(qg4p_m6 m6_demo.c demo_setup.c demo_images.c)           # images
qg4p_test(qg4p_m7 m7_demo.c demo_setup.c ASSETS)                  # asset pack
qg4p_test(qg4p_m8 m8_demo.c demo_setup.c demo_images.c)           # framebuffer
qg4p_test(qg4p_new_commands new_commands_demo.c demo_setup.c demo_images.c)  # VIEW, GET/PUT, styles
