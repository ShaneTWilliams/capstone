cmake_minimum_required(VERSION 3.12)

include(lib/pico-sdk/pico_sdk_init.cmake)
include(lib/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)
include(lib/pico-sdk/lib/tinyusb/hw/bsp/family_support.cmake)
include(lib/nanopb/extra/FindNanopb.cmake)

project(notch_serial_tool C CXX ASM)

set(ENV{PICO_PLATFORM} rp2040)
set(CMAKE_C_STANDARD 11)
set(ENV{CC} arm-none-eabi-gcc)

set(TARGET "app" CACHE STRING "Either 'boot' or 'app'")

pico_sdk_init()

add_compile_options(
    -Wall
    -Og
)

include_directories(
    ${NANOPB_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
)

nanopb_generate_cpp(VALUES_PROTO_SRCS VALUES_PROTO_HDRS ../proto/values.proto)
nanopb_generate_cpp(APP_PROTO_SRCS APP_PROTO_HDRS ../proto/app.proto)
nanopb_generate_cpp(BL_PROTO_SRCS BL_PROTO_HDRS ../proto/boot.proto)

message(STATUS "Building ${TARGET}")
if(${TARGET} STREQUAL "app")
    add_executable(${TARGET}
        src/app/main.c
        src/app/usb_descriptors.c
        src/app/fatal.c
        src/app/modules/adc.c
        src/app/modules/indication.c
        src/app/modules/usb.c
        src/app/generated/values.c
        ${APP_PROTO_SRCS}
        ${APP_PROTO_HDRS}
        ${VALUES_PROTO_SRCS}
        ${VALUES_PROTO_HDRS}
    )
    pico_set_linker_script(${TARGET} ${CMAKE_CURRENT_LIST_DIR}/ld/application.ld)
    target_link_libraries(
        ${TARGET}
        pico_stdlib
        pico_unique_id
        hardware_pio
        hardware_adc
        FreeRTOS-Kernel
        FreeRTOS-Kernel-Heap4
        tinyusb_device
    )
    include_directories(
        src
        src/app
        src/app/modules
        src/app/generated
    )

elseif(${TARGET} STREQUAL "boot")
    add_executable(${TARGET}
        src/boot/main.c
        src/boot/usb_descriptors.c
        ${BL_PROTO_SRCS}
        ${BL_PROTO_HDRS}
    )
    include_directories(
        src/boot
    )
    target_link_libraries(${TARGET} pico_stdlib pico_unique_id tinyusb_device)
    pico_set_linker_script(${TARGET} ${CMAKE_CURRENT_LIST_DIR}/ld/bootloader.ld)
endif()

target_link_directories(${TARGET} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/ld)
pico_add_extra_outputs(${TARGET})
