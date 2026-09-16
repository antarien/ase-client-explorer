cmake_minimum_required(VERSION 3.22)

#[[
ASE CLIENT BUILD CONFIGURATION

@file        CMakeLists.txt
@brief       Build configuration for ase-client-explorer
@description Native GTK4 hierarchical project explorer with NerdFont icons and DnD.

@module      ase-client-explorer
@layer       5 (Clients)
@created     2026-04-12
@modified    2026-09-08
@version     00.00.26.00026 [init]
]]

# Parse local VERSION (SSOT) if not building from root
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

# ── Foundation dependencies (built standalone if not already in scope) ──

if(NOT TARGET ase::utils)
    set(ASE_FOUNDATION_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../foundation)
    add_subdirectory(${ASE_FOUNDATION_PATH}/ase-utils ${CMAKE_BINARY_DIR}/_deps/ase-utils-build)
endif()

if(NOT TARGET ase::fileio)
    set(ASE_FOUNDATION_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../foundation)
    add_subdirectory(${ASE_FOUNDATION_PATH}/ase-fileio ${CMAKE_BINARY_DIR}/_deps/ase-fileio-build)
endif()

if(NOT TARGET ase::containers)
    set(ASE_FOUNDATION_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../foundation)
    add_subdirectory(${ASE_FOUNDATION_PATH}/ase-containers ${CMAKE_BINARY_DIR}/_deps/ase-containers-build)
endif()

if(NOT TARGET ase::math)
    set(ASE_FOUNDATION_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../foundation)
    add_subdirectory(${ASE_FOUNDATION_PATH}/ase-math ${CMAKE_BINARY_DIR}/_deps/ase-math-build)
endif()

if(NOT TARGET ase::json)
    # ase-json is a header-only INTERFACE wrapper around nlohmann_json;
    # pulling it in standalone requires the nlohmann_json target to exist
    # first (the parent root-build provides it via FetchContent). Prefer
    # the system-installed package for the standalone explorer build.
    if(NOT TARGET nlohmann_json::nlohmann_json)
        find_package(nlohmann_json QUIET)
        if(NOT nlohmann_json_FOUND)
            include(FetchContent)
            FetchContent_Declare(
                json
                GIT_REPOSITORY https://github.com/nlohmann/json.git
                GIT_TAG v3.11.3
            )
            FetchContent_MakeAvailable(json)
        endif()
    endif()
    set(ASE_FOUNDATION_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../foundation)
    add_subdirectory(${ASE_FOUNDATION_PATH}/ase-json ${CMAKE_BINARY_DIR}/_deps/ase-json-build)
endif()

# ── Adapter dependencies (built standalone if not already in scope) ─────

if(NOT TARGET ase::adp::gtk)
    set(ASE_ADAPTER_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../adapter)
    add_subdirectory(${ASE_ADAPTER_PATH}/ase-adp-gtk ${CMAKE_BINARY_DIR}/_deps/ase-adp-gtk-build)
endif()

if(NOT TARGET ase::adp::libgit2)
    set(ASE_ADAPTER_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../adapter)
    add_subdirectory(${ASE_ADAPTER_PATH}/ase-adp-libgit2 ${CMAKE_BINARY_DIR}/_deps/ase-adp-libgit2-build)
endif()

if(NOT TARGET ase::adp::libcuckoo)
    set(ASE_ADAPTER_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../adapter)
    add_subdirectory(${ASE_ADAPTER_PATH}/ase-adp-libcuckoo ${CMAKE_BINARY_DIR}/_deps/ase-adp-libcuckoo-build)
endif()

# ── Tests ────────────────────────────────────────────────────────────
#
# `enable_testing()` MUSS vor den Zielen stehen: ohne den Aufruf merkt sich CMake keine der
# add_test()-Deklarationen, und `ctest` sieht eine leere Suite. Beim Bau mit `ase exp -B` ist
# dieses Verzeichnis die Projektwurzel, also gilt hier derselbe Satz wie in den fuenf Tiers.
#
# DER BLOCK IST DER ANSCHLUSS, NICHT DIE ABLAGE. Gemessen 2026-09-16: 32 der 139 Einheiten des
# Baums fuehren ihn ohne eigenes tests/ — darunter alle fuenf Tiers und die ganze Foundation.
if(ASE_BUILD_TESTS)
    enable_testing()
endif()

# ── Desktop Executable ───────────────────────────────────────────────
#
# SOLID/SSOT layout: main.cpp is a pure orchestrator (~80 lines). Every
# feature lives in its own translation unit with a single responsibility
# under include/explorer and src/. Add new features by dropping a new
# .cpp file here and an .hpp file under include/explorer.

add_executable(ase-explorer
    src/main.cpp
    src/icons.cpp
    src/exclude.cpp
    src/submodule.cpp
    src/theme.cpp
    src/search_bar.cpp
    src/breadcrumb.cpp
    src/file_watcher.cpp
    src/clipboard_ops.cpp
    src/context_menu.cpp
    src/keyboard_shortcuts.cpp
    src/settings_dialog.cpp
    src/file_associations.cpp
    src/explorer_settings.cpp
    src/open_with_dialog.cpp
    src/folder_picker.cpp
    src/app_catalog.cpp
    src/extension_scan.cpp
    src/tree_view.cpp
    src/window.cpp
    src/git_status.cpp
    src/git_scan_pool.cpp

    include/explorer/types.hpp
    include/explorer/icons.hpp
    include/explorer/exclude.hpp
    include/explorer/submodule.hpp
    include/explorer/theme.hpp
    include/explorer/search_bar.hpp
    include/explorer/breadcrumb.hpp
    include/explorer/file_watcher.hpp
    include/explorer/clipboard_ops.hpp
    include/explorer/context_menu.hpp
    include/explorer/keyboard_shortcuts.hpp
    include/explorer/settings_dialog.hpp
    include/explorer/file_associations.hpp
    include/explorer/explorer_settings.hpp
    include/explorer/open_with_dialog.hpp
    include/explorer/folder_picker.hpp
    include/explorer/app_catalog.hpp
    include/explorer/extension_scan.hpp
    include/explorer/tree_view.hpp
    include/explorer/window.hpp
    include/explorer/git_status.hpp
    include/explorer/git_scan_pool.hpp
)

# ZWEI WARNUNGEN DES CMAKE-TORES STEHEN HIER BEWUSST OFFEN:
# `target_include_directories should use generator expressions` und
# `install(DIRECTORY include/ ...) for headers`. Beide stehen auch an tools/ase-cli und
# tools/ase-edge-daemon, die dort je noch zwei weitere tragen.
#
# BEIDE BESCHREIBEN DEN VERTRAG EINER BIBLIOTHEK. Dieses Ziel ist ein Executable: die
# Include-Pfade sind PRIVATE, weil kein fremdes Ziel dagegen bindet, und `$<INSTALL_INTERFACE:>`
# an einem PRIVATE-Schluesselwort ist folgenlos. Ein `install(DIRECTORY include/)` legte die
# privaten Kopfdateien einer Anwendung ins Systempraefix.
#
# Die zwei Zeilen einzutragen faerbt das Tor gruen und macht die Datei unwahr. GEMESSEN
# 2026-09-16: keines der drei ASE-Executables traegt sie, und keines der fuenf Tiers — es ist
# eine KLASSE, kein Befund dieser Einheit.
target_include_directories(ase-explorer
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../sha-client-web/sha-web-console/generated
)

# Target-lokale Sprachstufe. `set(CMAKE_CXX_STANDARD 20)` weiter oben setzt sie
# verzeichnisweit und bleibt stehen; diese Zeile bindet sie zusaetzlich AN DAS ZIEL,
# damit sie eine Eigenschaft von ase-explorer ist und nicht des Verzeichnisses, in dem
# es gerade konfiguriert wird. Gleiche Form wie tools/ase-cli:101 — PRIVATE, weil ein
# Executable seine Sprachstufe an niemanden weitergibt.
target_compile_features(ase-explorer PRIVATE cxx_std_20)

#[[ VERBOTEN: keine L3-Module, keine L4-Plugins, kein Kernel.
    Dieser Client ist L5 und bindet ausschliesslich Foundation (L0), Core (L1) und Adapter
    (ase::adp::*). Er laedt keine Module und fuehrt keinen Kernel — eine Kante nach modules/
    oder plugins/ waere keine Abkuerzung, sondern eine Abhaengigkeit auf einen Bestand, den
    diese Bau-Einheit gar nicht konfiguriert. ]]
target_link_libraries(ase-explorer
    PUBLIC
        ase::containers
    PRIVATE
        ase::adp::gtk
        ase::adp::libgit2
        ase::adp::libcuckoo
        ase::utils
        ase::fileio
        ase::json
        ase::math
)

set_target_properties(ase-explorer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ENABLE_EXPORTS ON  # -rdynamic so backtrace_symbols() resolves names
)

# Installation
include(GNUInstallDirs)
install(TARGETS ase-explorer DESTINATION ${CMAKE_INSTALL_BINDIR})
