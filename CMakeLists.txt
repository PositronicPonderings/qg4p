# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
# ---------------------------------------------------------------------------
#  QG4P development project: the library plus every test program.
#
#  To USE QG4P in your own project you only need the qg4p/ folder; see
#  qg4p/CMakeLists.txt. This file builds the whole repository.
#
#  Opening this folder with the Raspberry Pi Pico VS Code extension: the
#  extension may offer to add its own settings block at the top of this
#  file. That's fine; keep it above the lines below.
# ---------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(PICO_BOARD pico2 CACHE STRING "Board type")

include(pico_sdk_import.cmake)
project(qg4p C CXX ASM)
pico_sdk_init()

add_subdirectory(qg4p)             # the libraries: qg4p, qg4p_assets
add_subdirectory(examples)         # examples: qg4p_hello, ...
add_subdirectory(tests/hardware)   # hardware test programs qg4p_m0 ... qg4p_m8
