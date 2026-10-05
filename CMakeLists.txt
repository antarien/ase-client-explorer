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

# ── ASE_ROOT — die Wurzel, gegen die zugezogene Module ihre DATEN suchen ──
#
# EIN ZUGEZOGENES MODUL SIEHT DEN AUFRUFER ALS PROJEKTWURZEL, und das ist beim Standalone-Bau
# dieses Clients falsch. core/ase-log/CMakeLists.txt:64-70 sagt es ausdruecklich — „ASE_ROOT
# for standalone server builds, CMAKE_SOURCE_DIR for root build" — und bildet daraus den Pfad
# zu den Taxonomie-Daten seines Codegen-Schritts. Ohne diese Zeile setzte es
# CMAKE_SOURCE_DIR ein und suchte unter clients/ase-client-explorer/tools/ase-forge/… , wo
# nichts liegt; Ninja brach mit „missing and no known rule to make it" ab.
#
# DER FEHLER WAR AM CMAKE-LAUF NICHT ZU SEHEN: die Konfiguration lief durch, der Pfad entsteht
# als Abhaengigkeit eines Stempels und faellt erst beim Uebersetzen auf. Dieselbe Tiefe wie bei
# tools/ase-cli/CMakeLists.txt:38, das die Variable genauso bildet.
get_filename_component(ASE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

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

# ── Core dependency (L1) — die Log-Kante dieses Clients ────────────────
#
# WARUM DIESER CLIENT ase::log BINDET: er hatte keinen Fehlerkanal. Drei Dateikoepfe
# hielten die Luecke ausdruecklich fest und nannten zugleich die Bedingung, unter der sie
# zu schliessen ist — „wer sie schliessen will, braucht zuerst eine Log-Kante fuer diesen
# Client, nicht eine Zeile in dieser Datei" (explorer_settings.cpp, file_associations.cpp,
# git_scan_pool.cpp). Gemessen am Startweg: ein fehlgeschlagenes g_app_info_launch war an
# keiner Stelle sichtbar, weil die Zusage dieser Funktion beim ANGELEGTEN Prozess endet.
# Ein Betrachter, dessen Binaer am dynamischen Linker starb, blieb darum monatelang still.
#
# DIE BESCHAFFUNG FOLGT DER FORM DIESER DATEI, NICHT DER DER TIERS. cmake/Dependencies.cmake
# ruft FetchContent_MakeAvailable fuer ACHT Vorhaben in einer Zeile, libdatachannel und FTXUI
# darunter; eingebunden zoege dieser Dateibrowser einen WebRTC-Stack fuer eine Fehlzeile.
# Stattdessen dieselbe Form wie oben bei nlohmann_json: zuerst das Systempaket, FetchContent
# nur als Rueckfall. Gemessen liegt spdlog hier als 1.17.0, EnTT nicht — der Rueckfall trifft
# also genau eines der beiden, und welches, entscheidet die Maschine und nicht diese Datei.
#
# DIE REIHENFOLGE IST BINDEND: ase::log → ase::ecs, ase::containers ; ase::ecs → entt,
# ase::types. ase-log nennt spdlog::spdlog in seiner Bindeliste, BESCHAFFT es aber nicht —
# wer es ohne dieses Ziel per add_subdirectory zieht, bekommt keinen Schichtbruch gemeldet,
# sondern ein unbekanntes Ziel, und das ist der schlechtere Meldeweg.

# KEIN find_package FUER DIESE ZWEI — DAS HAUS BAUT SIE SELBST.
#
# Die erste Fassung suchte zuerst das Systempaket, nach dem Muster, das diese Datei bei
# nlohmann_json fuehrt. Fuer spdlog ist das falsch, und der Unterschied ist gemessen: alle
# fuenf Tiers holen spdlog ueber FetchContent und binden es STATISCH — kein Tier-Binaer
# nennt libspdlog in seinen NEEDED-Eintraegen. Das Systempaket ist dagegen eine
# Shared Library, die libfmt.so.12 nachzieht.
#
# UND GENAU DIESE NACHZIEHUNG SCHEITERT AUF DIESER MASCHINE. /opt/anaconda/lib fuehrt ein
# libfmt.so.11 und steht im Suchpfad vor /usr/lib, wo das passende libfmt.so.12 liegt; der
# Binder meldet es als „needed by /usr/lib/libspdlog.so.1.17, not found". CMake warnt vor
# derselben Verschattung bereits fuer sechs GTK-Bibliotheken — eine Warnung, die bisher
# niemanden betraf, weil kein Ziel dieses Clients eine verschattete Kette hatte.
#
# nlohmann_json darf das Systempaket nehmen, weil es HEADER-ONLY ist und nichts nachzieht.
# Die Unterscheidung ist nicht die Bequemlichkeit, sondern die Bauform des Pakets.
include(FetchContent)

if(NOT TARGET spdlog::spdlog)
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.13.0
    )
    FetchContent_MakeAvailable(spdlog)
endif()

if(NOT TARGET EnTT::EnTT)
    FetchContent_Declare(
        entt
        GIT_REPOSITORY https://github.com/skypjack/entt.git
        GIT_TAG v3.13.0
    )
    FetchContent_MakeAvailable(entt)
endif()

if(NOT TARGET ase::types)
    set(ASE_FOUNDATION_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../foundation)
    add_subdirectory(${ASE_FOUNDATION_PATH}/ase-types ${CMAKE_BINARY_DIR}/_deps/ase-types-build)
endif()

# ase::platform STAND IN KEINER DOKUMENTIERTEN KETTE UND BRACH DEN BAU TROTZDEM.
# core/ase-ecs/CMakeLists.txt:188 bindet es als PRIVATE — sleep_nanos() fuer die Boot- und
# Abbau-Taktung. Ein PRIVATE-Eintrag wird nicht an Verbraucher weitergereicht, aber das Ziel
# muss trotzdem IM BAUM stehen; sonst meldet CMake „links to ase::platform but the target was
# not found". Abgeleitet war diese Liste zuerst aus einem Kommentar in tools/ase-cli, der nur
# ase::ecs → entt, ase::types nennt, und genau diese Zeile fehlte dort. Die Bindeliste des
# Ziels ist nicht der Abschluss: gemessen wird an den CMakeLists, nie an ihrer Beschreibung.
#
# ase-platform ist ein L0-Blatt und bindet selbst kein ase::-Ziel (sein eigener Vermerk :102
# nennt eine ase::-Kante dort ausdruecklich falsch). Damit endet der Abschluss hier.
if(NOT TARGET ase::platform)
    set(ASE_FOUNDATION_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../foundation)
    add_subdirectory(${ASE_FOUNDATION_PATH}/ase-platform ${CMAKE_BINARY_DIR}/_deps/ase-platform-build)
endif()

if(NOT TARGET ase::ecs)
    set(ASE_CORE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../core)
    add_subdirectory(${ASE_CORE_PATH}/ase-ecs ${CMAKE_BINARY_DIR}/_deps/ase-ecs-build)
endif()

if(NOT TARGET ase::log)
    set(ASE_CORE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../core)
    add_subdirectory(${ASE_CORE_PATH}/ase-log ${CMAKE_BINARY_DIR}/_deps/ase-log-build)
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
        # ase::log (core/ase-log — L1): der EINE Fehlerkanal dieses Clients. Er traegt die
        # Kategorie-SSOT aus core/ase-log/data/log_categories.json mit; eine Meldung ohne
        # gueltige Kategorie uebersetzt nicht.
        ase::log
)

set_target_properties(ase-explorer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ENABLE_EXPORTS ON  # -rdynamic so backtrace_symbols() resolves names
)

# Installation
include(GNUInstallDirs)
install(TARGETS ase-explorer DESTINATION ${CMAKE_INSTALL_BINDIR})
