cmake_minimum_required(VERSION 3.20)
project(pifmrds-enchanted VERSION 2.1.0 LANGUAGES C)

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

install(PROGRAMS tools/pifm-ctl.py
    DESTINATION bin
    RENAME pifm-ctl
)

install(FILES tools/pifm-enchanted.service
    DESTINATION lib/systemd/system OPTIONAL
)

install(DIRECTORY doc/
    DESTINATION share/doc/pifmrds-enchanted
)

# CPack Packaging (Debian .deb & tarball)
set(CPACK_PACKAGE_NAME "pifmrds-enchanted")
set(CPACK_PACKAGE_VENDOR "Marcel Siepielski")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Modern FM/RDS transmitter with MPX stereo, RT+, and SDR support")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_GENERATOR "TGZ")

if (UNIX AND NOT APPLE)
    list(APPEND CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "deloskiytbackup <mkrupelski@gmail.com>")
    set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/deloskiytbackup/PiFmRds-Enchanted")
endif()

include(CPack)
