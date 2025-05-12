cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME device)

# Include toolchain file
include("cmake/gcc-arm-none-eabi.cmake")

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

target_link_options(${CMAKE_PROJECT_NAME} PRIVATE -u _printf_float)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    src/adc.c
    src/device.c
    src/parsers.c
    src/impl/sim800_impl.c
    src/impl/discrete_input_impl.c
    src/impl/discrete_output_impl.c
    src/impl/eeprom_in_flash_impl.c
    src/RTT/SEGGER_RTT.c
    src/RTT/SEGGER_RTT_printf.c
    src/RTT/SEGGER_RTT_ASM_ARMv7M.S
    lib/sim800/sim800.c
    lib/sim800/sim800_sms.c
    lib/sim800/utf8_xcoder.c
    lib/sim800/sim800_parser.c
    lib/sim800/sim800_AT_cmd.c
    lib/discrete_io/discrete_input.c
    lib/discrete_io/discrete_output.c
    lib/circular_buffer/circular_buffer.c
    lib/eeprom_in_flash/eeprom_in_flash.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    inc
    inc/conf
    lib/sim800
    lib/discrete_io
    lib/circular_buffer
    lib/eeprom_in_flash
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)

add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O ihex
        $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
        ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.hex
        COMMENT "Generating HEX file"
)
