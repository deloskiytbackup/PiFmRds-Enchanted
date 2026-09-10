cmake_minimum_required(VERSION 3.20)
project(pifmrds-enchanted VERSION 2.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler warnings
if (CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_subdirectory(src)

# Testing
option(BUILD_TESTING "Build unit tests" ON)
if (BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation
install(TARGETS pi_fm_rds rds_wav
    RUNTIME DESTINATION bin
)

install(FILES tools/pifm-enchanted.service
    DESTINATION /etc/systemd/system OPTIONAL
)
