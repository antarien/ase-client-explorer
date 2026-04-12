#!/bin/bash
# ==============================================================================
# ASE Project Explorer — Layer 5 Client
# Professional Build Script with Ninja
# ==============================================================================

# NOTE: Do NOT use "set -e" here. This script handles errors explicitly.

BUILD_START_TIME=$(date +%s)

# Terminal output framework (SSOT: sha-client-web/sha-web-console)
_CONSOLE_SH="$(dirname "${BASH_SOURCE[0]}")/../sha-client-web/public/console/bash/console.sh"
if [ -f "$_CONSOLE_SH" ]; then
    source "$_CONSOLE_SH"
else
    # Fallback: minimal output
    NC='\033[0m'; RED='\033[31m'; GREEN='\033[32m'; CYAN='\033[36m'; YELLOW='\033[33m'
    MAGENTA='\033[35m'; BLUE='\033[34m'; GRAY='\033[90m'; PURPLE='\033[35m'
    MUTED='\033[90m'; TEXT='\033[37m'
    CHECK="✓"; CROSS="✗"; HASH="#"; ARROW="→"; SKIP="○"
    SEC_COLOR="$MUTED"
    TERM_WIDTH=${COLUMNS:-120}
    section_header() { echo -e "\n  ${CYAN}┌─ $1 $( printf '─%.0s' $(seq 1 $((${2:-60} - ${#1} - 4))) )${NC}"; }
    section_line() { echo -e "  ${SEC_COLOR}│${NC} $1  $2  $3"; }
    section_pipe() { echo -e "  ${SEC_COLOR}│${NC}"; }
    section_detail() { echo -e "  ${SEC_COLOR}│${NC}     $1"; }
    section_spin() { local lbl="$1"; shift; local out; out=$("$@" 2>&1); SPIN_EXIT=$?; SPIN_OUT="$out"; }
fi

# CPU/IO priority
BUILD_JOBS=$(( $(nproc) - 2 ))
[ "$BUILD_JOBS" -lt 2 ] && BUILD_JOBS=2
BUILD_PREFIX="nice -n 10 ionice -c2 -n 7"

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
VERSION_FILE="$SCRIPT_DIR/VERSION"

# ============================================
# VERSION MANAGEMENT
# ============================================

get_version() {
    local key="$1"
    if [ -f "$VERSION_FILE" ]; then
        local result
        result=$(grep "^${key}=" "$VERSION_FILE" 2>/dev/null | cut -d'=' -f2)
        echo "${result:-00.00.00.00000}"
    else
        echo "00.00.00.00000"
    fi
}

get_build() {
    if [ -f "$VERSION_FILE" ]; then
        local version
        version=$(grep "^ASE_CLIENT_EXPLORER=" "$VERSION_FILE" 2>/dev/null | cut -d'=' -f2)
        echo "${version##*.}"
    else
        echo "00001"
    fi
}

get_status() {
    if [ -f "$VERSION_FILE" ]; then
        local result
        result=$(grep "^ASE_CLIENT_EXPLORER_STATUS=" "$VERSION_FILE" 2>/dev/null | cut -d'=' -f2)
        echo "${result:-stub}"
    else
        echo "stub"
    fi
}

bump_build() {
    if [ -f "$VERSION_FILE" ]; then
        local current
        current=$(get_build)
        local new_build=$((10#$current + 1))
        local major=0
        local minor=$((new_build / 50))
        local patch=$((new_build % 50))
        local new_version
        new_version=$(printf "%02d.%02d.%02d.%05d" $major $minor $patch $new_build)
        sed -i "s/^ASE_CLIENT_EXPLORER=.*/ASE_CLIENT_EXPLORER=${new_version}/" "$VERSION_FILE"
        sed -i "s/^ASE_CLIENT_EXPLORER_UPDATED=.*/ASE_CLIENT_EXPLORER_UPDATED=$(date +%Y-%m-%d)/" "$VERSION_FILE"
        printf "%05d" "$new_build"
    fi
}

get_status_color() {
    local status_val="$1"
    case "$status_val" in
        stub)       echo "${GRAY}" ;;
        poc)        echo "${MAGENTA}" ;;
        init|core|feat) echo "${YELLOW}" ;;
        refine|alpha)   echo "${CYAN}" ;;
        beta)       echo "${BLUE}" ;;
        stable)     echo "${CYAN}" ;;
        *)          echo "${GRAY}" ;;
    esac
}

get_status_icon() {
    local status_val="$1"
    case "$status_val" in
        stub) echo "○" ;; poc) echo "◐" ;; init) echo "◑" ;; core) echo "◒" ;;
        feat) echo "◓" ;; refine) echo "●" ;; alpha) echo "α" ;; beta) echo "β" ;;
        stable) echo "★" ;; *) echo "?" ;;
    esac
}

VERSION=$(get_version "ASE_CLIENT_EXPLORER")
BUILD_NUM=$(get_build)
STATUS=$(get_status)
STATUS_COLOR=$(get_status_color "$STATUS")
STATUS_ICON=$(get_status_icon "$STATUS")

# ============================================
# CLI FLAGS
# ============================================

DO_RUN=false
IS_CLEAN=false
BUILD_TYPE="${BUILD_TYPE:-Release}"
BUILD_DIR="${SCRIPT_DIR}/build"
RUN_ARGS=()

for arg in "$@"; do
    case "$arg" in
        run)   DO_RUN=true ;;
        clean) IS_CLEAN=true ;;
        debug) BUILD_TYPE="Debug" ;;
        --help|-h)
            echo -e "${MAGENTA}ase-client-explorer — Project Explorer (L5 Client)${NC}"
            echo -e "  ${CYAN}./build.sh${NC}         Release build"
            echo -e "  ${CYAN}./build.sh run${NC}     Build + run"
            echo -e "  ${CYAN}./build.sh debug${NC}   Debug build"
            echo -e "  ${CYAN}./build.sh clean${NC}   Clean build cache"
            exit 0 ;;
        *)     RUN_ARGS+=("$arg") ;;
    esac
done

# ============================================
# HEADER
# ============================================

section_header "ASE Project Explorer" 170
section_line "·" "Native GTK4 hierarchical project explorer"
section_line "·" "NerdFont file icons, DnD, xdg-open"
section_pipe
section_line "*" "Version" "v${VERSION} [${STATUS}]"
section_line "$HASH" "Build" "${BUILD_TYPE} (${BUILD_JOBS} threads)"

# ============================================
# CLEAN
# ============================================

if [ "$IS_CLEAN" = true ]; then
    section_header "Clean (preserving _deps)" 71
    if [ -d "$BUILD_DIR" ]; then
        cd "$BUILD_DIR"
        find . -name "*.ninja*" -not -path "./_deps/*" -delete 2>/dev/null || true
        find . -name "CMakeCache.txt" -not -path "./_deps/*" -delete 2>/dev/null || true
        find . -name "CMakeFiles" -type d -not -path "./_deps/*" -exec rm -rf {} + 2>/dev/null || true
        rm -rf bin lib 2>/dev/null || true
        cd "$SCRIPT_DIR"
        section_line "$CHECK" "Build cache cleaned"
    else
        section_line "$SKIP" "Nothing to clean"
    fi
fi

# ============================================
# BUILD
# ============================================

mkdir -p "$BUILD_DIR"

section_header "Build" 214

# CMake configure (only if needed)
if [ ! -f "$BUILD_DIR/build.ninja" ] || [ -n "$(find "$SCRIPT_DIR" -name 'CMakeLists.txt' -newer "$BUILD_DIR/build.ninja" 2>/dev/null | head -1)" ]; then
    section_line "$ARROW" "CMake configure"
    $BUILD_PREFIX cmake -B "$BUILD_DIR" -G Ninja \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -S "$SCRIPT_DIR" 2>&1 | tail -5
    if [ ${PIPESTATUS[0]} -ne 0 ]; then
        section_line "$CROSS" "CMake configure failed"
        exit 1
    fi
    section_line "$CHECK" "CMake configured"
else
    section_line "$SKIP" "CMake up to date"
fi

# Ninja build
section_line "$ARROW" "Ninja build"
$BUILD_PREFIX ninja -C "$BUILD_DIR" -j "$BUILD_JOBS" 2>&1 | tail -3
if [ ${PIPESTATUS[0]} -ne 0 ]; then
    section_line "$CROSS" "Build failed"
    exit 1
fi

NEW_BUILD=$(bump_build)
section_line "$CHECK" "Build #${NEW_BUILD} complete"

# ============================================
# RUN
# ============================================

if [ "$DO_RUN" = true ]; then
    section_header "Run" 71
    section_line "$ARROW" "Starting ase-explorer"
    "$BUILD_DIR/bin/ase-explorer" "${RUN_ARGS[@]}"
fi

# ============================================
# SUMMARY
# ============================================

BUILD_END_TIME=$(date +%s)
BUILD_DURATION=$((BUILD_END_TIME - BUILD_START_TIME))
section_pipe
section_line "$CHECK" "Done in ${BUILD_DURATION}s"
echo ""
