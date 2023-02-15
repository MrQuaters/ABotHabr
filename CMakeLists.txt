cmake_minimum_required (VERSION 3.12)
project (gpio_output)

# Platform initialization
include (platform)
initialize_platform ()


# Installing
include (platform/install)


# Tests
include (platform/tests)
add_project_tests_main_target ()


# Platform definition
include (platform/options)

find_package (bsp REQUIRED)
include_directories (${bsp_INCLUDE})

# This command will find GPIO client lib
find_package (gpio REQUIRED COMPONENTS CLIENT_LIB ENTITY)
include_directories (${gpio_INCLUDE})

# Project - gpio
add_subdirectory (client)
add_subdirectory (einit)
