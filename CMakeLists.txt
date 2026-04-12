cmake_minimum_required(VERSION 3.22)

#[[
ASE CLIENT BUILD CONFIGURATION

@file        CMakeLists.txt
@brief       Build configuration for ase-client-explorer
@description Native GTK4 hierarchical project explorer with NerdFont icons and DnD.

@module      ase-client-explorer
@layer       5 (Clients)
@created     2026-04-12
@modified    2026-04-12
@version     00.00.01.00001 [stub]
]]

# Parse local VERSION if not building from root
if(NOT DEFINED ASE_CLIENT_EXPLORER_VERSION)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
        message(FATAL_ERROR "VERSION file not found: ${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
    endif()
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" _VERSION_LINES)
    foreach(_LINE ${_VERSION_LINES})
        if(_LINE MATCHES "^ASE_CLIENT_EXPLORER=(.*)$")
            set(ASE_CLIENT_EXPLORER_VERSION "${CMAKE_MATCH_1}")
        endif()
    endforeach()
    if(NOT DEFINED ASE_CLIENT_EXPLORER_VERSION)
        message(FATAL_ERROR "ASE_CLIENT_EXPLORER version not found in VERSION file")
    endif()
endif()

project(ase-client-explorer VERSION ${ASE_CLIENT_EXPLORER_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Generate version.hpp from template (SSOT)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/explorer/version.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/explorer/version.hpp"
    @ONLY
)

message(STATUS "Configuring ${PROJECT_NAME} v${PROJECT_VERSION}")

# ── System Dependencies ──────────────────────────────────────────────

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED IMPORTED_TARGET gtkmm-4.0)

# ── Desktop Executable ───────────────────────────────────────────────

add_executable(ase-explorer
    src/main.cpp
)

target_include_directories(ase-explorer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}/include
)

target_link_libraries(ase-explorer
    PRIVATE
        PkgConfig::GTKMM
)

set_target_properties(ase-explorer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Installation
include(GNUInstallDirs)
install(TARGETS ase-explorer DESTINATION ${CMAKE_INSTALL_BINDIR})
