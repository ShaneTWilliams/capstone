cmake_minimum_required(VERSION 3.12)

include(lib/pico-sdk/pico_sdk_init.cmake)
include(lib/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)
include(lib/pico-sdk/lib/tinyusb/hw/bsp/family_support.cmake)
include(lib/nanopb/extra/FindNanopb.cmake)

project(capstone C CXX ASM)

set(ENV{PICO_PLATFORM} rp2040)
set(CMAKE_C_STANDARD 11)
set(ENV{CC} arm-none-eabi-gcc)

set(TARGET "app" CACHE STRING "Either 'boot' or 'app'")

pico_sdk_init()

add_compile_options(
    -Wall
    -O0
    -Werror
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
        src/app/hw_config.c
        src/app/iox.c
        src/app/modules/adc.c
        src/app/modules/indication.c
        src/app/modules/usb.c
        src/app/modules/dma.c
        src/app/modules/imu.c
        src/app/modules/lp_i2c.c
        src/app/modules/hp_i2c.c
        src/app/modules/radio.c
        src/app/modules/gps.c
        src/app/modules/motor.c
        src/app/modules/state.c
        src/app/generated/values.c
        src/app/generated/bq25703a.c
        src/app/generated/is31fl3197.c
        src/app/generated/lm75b.c
        src/app/generated/lsm6dsm.c
        src/app/generated/max17320.c
        src/app/generated/mct8329a.c
        src/app/generated/tca9539.c
        src/app/generated/sx1262.c
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
        hardware_spi
        hardware_dma
        hardware_pwm
        hardware_i2c
        hardware_uart
        hardware_clocks
        FreeRTOS-Kernel
        FreeRTOS-Kernel-Heap4
        tinyusb_device
    )
    include_directories(
        src
        src/app
        src/app/modules
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

elseif(${TARGET} STREQUAL "radio")
    add_executable(${TARGET}
        src/radio/main.c
        src/app/generated/sx1262.c
    )
    include_directories(
        src/radio
        src/app
    )
    target_link_libraries(${TARGET} pico_stdlib pico_unique_id pico_time hardware_spi hardware_uart)
    pico_set_linker_script(${TARGET} ${CMAKE_CURRENT_LIST_DIR}/ld/application.ld)
    pico_enable_stdio_usb(${TARGET} 0)
    pico_enable_stdio_uart(${TARGET} 0)
endif()

target_link_directories(${TARGET} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/ld)
pico_add_extra_outputs(${TARGET})
