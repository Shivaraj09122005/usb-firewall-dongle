cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()

set(sdkVersion 2.3.1)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)

if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()

set(PICO_BOARD pico CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(pico-host C CXX ASM)

pico_sdk_init()

add_library(tinyusb_config INTERFACE)

target_include_directories(tinyusb_config INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}
)

add_executable(pico-host
        pico-host.c
)

pico_set_program_name(pico-host "pico-host")
pico_set_program_version(pico-host "0.1")

pico_enable_stdio_uart(pico-host 0)
pico_enable_stdio_usb(pico-host 0)

target_link_libraries(pico-host
        pico_stdlib
        tinyusb_host
        tinyusb_board
)

target_include_directories(pico-host PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

pico_add_extra_outputs(pico-host)
