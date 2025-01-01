# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(Firmware_Demo C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(Firmware_Demo main.c )

pico_set_program_name(Firmware_Demo "Firmware_Demo")
pico_set_program_version(Firmware_Demo "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(Firmware_Demo 0)
pico_enable_stdio_usb(Firmware_Demo 0)

# Add the standard library to the build
target_link_libraries(Firmware_Demo
        pico_stdlib
        pico_unique_id pico_stdio_usb hardware_pwm hardware_irq
        tinyusb_device tinyusb_board)

# add source directories
target_sources(Firmware_Demo PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/main.c
    ${CMAKE_CURRENT_LIST_DIR}/usb_descriptors.c
    ${CMAKE_CURRENT_LIST_DIR}/usbHandler.c
    ${CMAKE_CURRENT_LIST_DIR}/fftBin.c
    ${CMAKE_CURRENT_LIST_DIR}/Adafruit_ZeroFFT/arm_common_tables.c
    ${CMAKE_CURRENT_LIST_DIR}/Adafruit_ZeroFFT/fftwindows.c
    ${CMAKE_CURRENT_LIST_DIR}/Adafruit_ZeroFFT/fftutil.c
)

# Add the standard include files to the build
target_include_directories(Firmware_Demo PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/Adafruit_ZeroFFT
)

# Add any user requested libraries
target_link_libraries(Firmware_Demo 
        )

pico_add_extra_outputs(Firmware_Demo)

add_definitions(
    -DPICO_STDIO_USB_ENABLE_RESET_VIA_VENDOR_INTERFACE
    -DPICO_STDIO_USB_RESET_INTERFACE_SUPPORT_RESET_TO_BOOTSEL
    -DPICO_STDIO_USB_RESET_BOOTSEL_INTERFACE_DISABLE_MASK=0
    # -DSYS_CLK_HZ=126000000
    # -DPLL_SYS_VCO_FREQ_HZ=1512000000
    # -DPLL_SYS_POSTDIV1=6
    # -DPLL_SYS_POSTDIV2=2
    )

# add color terminal
add_definitions(-fdiagnostics-color=always)