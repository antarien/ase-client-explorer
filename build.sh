#!/bin/bash
# ==============================================================================
# ASE Project Explorer — Layer 5 Client
# Professional Build Script with Ninja
# ==============================================================================

# NOTE: Do NOT use "set -e" here. This script handles errors explicitly
# (cmake exit codes, ninja exit codes, validator exit codes).
# set -e causes silent deaths on harmless non-zero returns from grep, find,
# regex matching, printf, wait, etc. — making failures impossible to debug.

# Record start time for build duration
BUILD_START_TIME=$(date +%s)

# Terminal output framework (SSOT: sha-client-web/sha-web-console)
_CONSOLE_SH="$(dirname "${BASH_SOURCE[0]}")/../sha-client-web/public/console/bash/console.sh"
if [ -f "$_CONSOLE_SH" ]; then
    source "$_CONSOLE_SH"
else
    # Fallback: minimal output
    NC='\033[0m'; RED='\033[31m'; GREEN='\033[32m'; CYAN='\033[36m'; YELLOW='\033[33m'
    MAGENTA='\033[35m'; BLUE='\033[34m'; GRAY='\033[90m'; PURPLE='\033[35m'
    PINK='\033[38;5;205m'; ORANGE='\033[38;5;208m'
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

# CPU/IO priority: keep system responsive during builds
# Reserve 2 cores for desktop/browser (minimum 2 build threads)
BUILD_JOBS=$(( $(nproc) - 2 ))
[ "$BUILD_JOBS" -lt 2 ] && BUILD_JOBS=2
# nice -n 10: lower CPU priority (polite, not idle)
# ionice -c2 -n 7: best-effort I/O, lowest sub-priority
BUILD_PREFIX="nice -n 10 ionice -c2 -n 7"

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# VERSION file path
VERSION_FILE="$SCRIPT_DIR/VERSION"

# ============================================
# VERSION MANAGEMENT FUNCTIONS
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
        if [ -n "$version" ]; then
            echo "${version##*.}"
        else
            echo "00001"
        fi
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
        # Recalculate MM.mm.pp from build number (VERSION FORMAT SSOT)
        # MM = 00 (manual), mm = builds / 50, pp = builds % 50
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
        init)       echo "${YELLOW}" ;;
        core)       echo "${YELLOW}" ;;
        feat)       echo "${YELLOW}" ;;
        refine)     echo "${CYAN}" ;;
        alpha)      echo "${CYAN}" ;;
        beta)       echo "${BLUE}" ;;
        stable)     echo "${CYAN}" ;;
        *)          echo "${GRAY}" ;;
    esac
}

get_status_icon() {
    local status_val="$1"
    case "$status_val" in
        stub)       echo "○" ;;
        poc)        echo "◐" ;;
        init)       echo "◑" ;;
        core)       echo "◒" ;;
        feat)       echo "◓" ;;
        refine)     echo "●" ;;
        alpha)      echo "α" ;;
        beta)       echo "β" ;;
        stable)     echo "★" ;;
        *)          echo "?" ;;
    esac
}

# Read version info
VERSION=$(get_version "ASE_CLIENT_EXPLORER")
BUILD_NUM=$(get_build)
STATUS=$(get_status)
STATUS_COLOR=$(get_status_color "$STATUS")
STATUS_ICON=$(get_status_icon "$STATUS")

# ============================================
# HELP
# ============================================

if [ "$1" == "--help" ] || [ "$1" == "-h" ]; then
    echo -e "${MAGENTA}ASE Project Explorer — Native GTK4 Client${NC}"
    echo -e "${GRAY}Build Script with Ninja${NC}"
    echo ""
    echo -e "${BLUE}Usage:${NC}"
    echo -e "  ${CYAN}./build.sh${NC}              Release build"
    echo -e "  ${CYAN}./build.sh debug${NC}        Debug build"
    echo -e "  ${CYAN}./build.sh run${NC}          Build + run"
    echo -e "  ${CYAN}./build.sh debug run${NC}    Debug build + run"
    echo -e "  ${CYAN}./build.sh clean${NC}        Clean build (preserves _deps cache!)"
    echo -e "  ${CYAN}./build.sh fullclean${NC}    Full clean (removes everything)"
    echo -e "  ${CYAN}./build.sh clean run${NC}    Clean + build + run"
    echo ""
    echo -e "${BLUE}Build Specific Targets:${NC}"
    echo -e "  ${CYAN}./build.sh ase-explorer${NC}             Build main executable"
    echo -e "  ${CYAN}./build.sh targets${NC}                  List all available targets"
    echo -e "  ${CYAN}./build.sh targets ase-${NC}             List targets matching pattern"
    echo ""
    echo -e "${BLUE}Examples:${NC}"
    echo -e "  ${GRAY}./build.sh${NC}                           # Release build"
    echo -e "  ${GRAY}./build.sh run${NC}                       # Build + launch explorer"
    echo -e "  ${GRAY}./build.sh debug run${NC}                 # Debug build + launch"
    echo -e "  ${GRAY}./build.sh clean${NC}                     # Fast rebuild, deps cached"
    echo -e "  ${GRAY}./build.sh fullclean${NC}                 # Full clean, re-downloads"
    echo ""
    echo -e "${BLUE}Cached Dependencies:${NC}"
    echo -e "  ${CYAN}clean${NC} preserves _deps/ (fast rebuild)"
    echo -e "  ${RED}fullclean${NC} removes everything (slow, re-downloads)"
    echo ""
    exit 0
fi

# ============================================
# TARGETS LISTING
# ============================================

if [ "$1" == "targets" ]; then
    BUILD_DIR="${SCRIPT_DIR}/build"
    if [ ! -f "${BUILD_DIR}/build.ninja" ]; then
        section_header "Targets" 110
        section_line "$CROSS" "No build configured yet. Run ./build.sh first."
        echo ""
        exit 1
    fi

    FILTER="${2:-}"
    section_header "Explorer Targets" 110

    cd "$BUILD_DIR"
    if [ -n "$FILTER" ]; then
        section_line "$HASH" "Targets matching '${FILTER}':"
        section_pipe
        ninja -t targets 2>/dev/null | grep -E "^${FILTER}" | sed 's/: .*//' | sort | while read target; do
            section_line "$CHECK" "$target"
        done
    else
        section_line "$HASH" "All targets:"
        section_pipe
        ninja -t targets 2>/dev/null | sed 's/: .*//' | sort | while read target; do
            section_line "$CHECK" "$target"
        done
    fi
    echo ""
    exit 0
fi

# ============================================
# TARGET COLLECTION (for specific target builds)
# ============================================

BUILD_TARGETS=()
REMAINING_ARGS=()

for arg in "$@"; do
    if [[ "$arg" == ase-* ]] || [[ "$arg" == *"*"* && "$arg" != "fullclean" ]]; then
        BUILD_TARGETS+=("$arg")
    else
        REMAINING_ARGS+=("$arg")
    fi
done

# Reset positional parameters to non-target args
set -- "${REMAINING_ARGS[@]}"

# ============================================
# CLI FLAGS
# ============================================

DO_RUN=false
IS_CLEAN=false
IS_FULLCLEAN=false
BUILD_TYPE="${BUILD_TYPE:-Release}"
BUILD_DIR="${SCRIPT_DIR}/build"
RUN_ARGS=()

for arg in "$@"; do
    case "$arg" in
        run)       DO_RUN=true ;;
        clean)     IS_CLEAN=true ;;
        fullclean) IS_FULLCLEAN=true ;;
        debug)     BUILD_TYPE="Debug" ;;
        *)         RUN_ARGS+=("$arg") ;;
    esac
done

BUILD_MODE="CLI"
if [ "$BUILD_TYPE" = "Debug" ]; then
    BUILD_MODE="CLI-Debug"
fi

# ============================================
# HEADER
# ============================================

section_header "ASE Project Explorer" 170
section_line "·" "Native GTK4 hierarchical project explorer"
section_line "·" "NerdFont file icons, DnD, xdg-open"
section_pipe
section_line "*" "Version" "v${VERSION} [${STATUS}]"
section_line "$HASH" "Build Mode" "${BUILD_MODE} (${BUILD_TYPE})"

# ============================================
# PRE-BUILD VALIDATION PHASE
# ============================================
# NOTE: Root ECS validators (validate_header_includes.py, validate_fields.py, etc.)
# operate on modules/plugins/core via --all/--module flags.
# Standalone L5 clients are outside their scan scope.
# Client-specific validators can be added here when needed.

# ============================================
# CLEAN PHASE
# ============================================

if [ "$IS_FULLCLEAN" = true ]; then
    section_header "Full Clean" 71
    section_line "x" "Removing entire build directory"
    section_detail "Path: $BUILD_DIR"

    rm -rf "$BUILD_DIR" 2>/dev/null || true

    section_line "$CHECK" "Build directory removed completely"

elif [ "$IS_CLEAN" = true ]; then
    section_header "Clean (preserving _deps)" 71

    if [ -d "$BUILD_DIR" ]; then
        section_line "x" "Cleaning build cache"
        section_detail "Path: $BUILD_DIR"

        cd "$BUILD_DIR"
        # Remove build files but KEEP _deps downloads for speed!
        find . -name "*.ninja*" -not -path "./_deps/*" -delete 2>/dev/null || true
        find . -name "CMakeCache.txt" -not -path "./_deps/*" -delete 2>/dev/null || true
        find . -name "CMakeFiles" -type d -not -path "./_deps/*" -exec rm -rf {} + 2>/dev/null || true
        # Remove compiled artifacts
        rm -rf bin lib 2>/dev/null || true
        cd "$SCRIPT_DIR"

        # Count cached deps
        DEPS_COUNT=$(find "$BUILD_DIR/_deps" -maxdepth 1 -type d 2>/dev/null | wc -l)
        DEPS_COUNT=$((DEPS_COUNT - 1))

        section_line "$CHECK" "Build cache cleaned"
        if [ $DEPS_COUNT -gt 0 ]; then
            section_line "$CHECK" "Preserved $DEPS_COUNT cached dependencies"
        fi
    else
        section_line "$SKIP" "Nothing to clean"
    fi
fi

# ============================================
# CREATE BUILD DIRECTORY
# ============================================

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# ============================================
# CMAKE CONFIGURATION PHASE
# ============================================

section_header "Build" 214

# SMART: Only run CMake if build.ninja doesn't exist or build files changed
# Wrapped in spinner because find scans entire project tree
_check_cmake_needed() {
    if [ ! -f "build.ninja" ]; then
        echo "true:initial configuration"
    else
        local _changed_cmake
        _changed_cmake=$(find "$SCRIPT_DIR" -name 'CMakeLists.txt' -newer build.ninja 2>/dev/null | head -1)
        if [ -n "$_changed_cmake" ]; then
            echo "true:CMakeLists.txt changed"
        else
            local _changed_header
            _changed_header=$(find "$SCRIPT_DIR/include" \
                -name '*.hpp' -newer build.ninja 2>/dev/null | head -1)
            if [ -n "$_changed_header" ]; then
                echo "true:header changed ($(basename "$_changed_header"))"
            else
                echo "false:"
            fi
        fi
    fi
}

_cmake_tmp=$(mktemp)
_check_cmake_needed > "$_cmake_tmp" &
_cmake_pid=$!
_spin='⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏'
_si=0
while kill -0 "$_cmake_pid" 2>/dev/null; do
    printf "\r  ${SEC_COLOR}│${NC} ${MUTED}${_spin:$_si:1}${NC}  ${MUTED}CMake${NC}\033[K"
    _si=$(( (_si + 1) % 10 ))
    sleep 0.08
done
wait "$_cmake_pid" 2>/dev/null
printf "\r\033[K"

_cmake_result=$(cat "$_cmake_tmp")
rm -f "$_cmake_tmp"
NEEDS_CMAKE="${_cmake_result%%:*}"
CMAKE_REASON="${_cmake_result#*:}"

if [ "$NEEDS_CMAKE" = "false" ]; then
    section_line "$CHECK" "CMake" "cached (build.ninja up-to-date)" 18
fi

if [ "$NEEDS_CMAKE" = true ]; then
    STEP_START=$(date +%s%3N)

    # Spinner while cmake runs
    section_spin "CMake  ${CMAKE_REASON:-configuration}" \
        $BUILD_PREFIX cmake -G Ninja "$SCRIPT_DIR" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -Wno-dev
    CMAKE_OUTPUT="$SPIN_OUT"
    CMAKE_RESULT=$SPIN_EXIT

    section_line "$HASH" "CMake" "${CMAKE_REASON:-configuration}" 18
    section_detail "Generator: Ninja"
    section_detail "Build Type: ${BUILD_TYPE}"

    # Show filtered output with proper box-drawing formatting
    echo "$CMAKE_OUTPUT" | while read -r line; do
        # Module configuration lines
        if [[ "$line" =~ ^"-- Configuring "(ase-[a-z0-9-]+)" v"([0-9.]+) ]]; then
            module="${BASH_REMATCH[1]}"
            version="${BASH_REMATCH[2]}"
            echo -e "  ${SEC_COLOR}│${NC}       ${CYAN}+${NC} ${CYAN}${module}${NC} ${MUTED}v${version}${NC}"
        # CMake Errors (red)
        elif [[ "$line" =~ "CMake Error" ]] || [[ "$line" =~ "FATAL_ERROR" ]]; then
            content="${line#-- }"
            echo -e "  ${SEC_COLOR}│${NC}       ${RED}${CROSS} ${content}${NC}"
        # CMake Warnings (yellow)
        elif [[ "$line" =~ "CMake Warning" ]]; then
            content="${line#-- }"
            echo -e "  ${SEC_COLOR}│${NC}       ${YELLOW}! ${content}${NC}"
        # Found package lines
        elif [[ "$line" =~ ^"-- Found " ]]; then
            pkg="${line#-- Found }"
            echo -e "  ${SEC_COLOR}│${NC}       ${CHECK} ${TEXT}${pkg}${NC}"
        fi
    done

    # Check for errors
    if [ $CMAKE_RESULT -ne 0 ] || echo "$CMAKE_OUTPUT" | grep -qE "(Configuring incomplete|Parse error|CMake Error)"; then
        section_pipe
        section_line "$CROSS" "CMake configuration failed!"
        echo ""
        echo -e "${RED}Full CMake output:${NC}"
        echo "$CMAKE_OUTPUT" | tail -100
        exit 1
    fi

    STEP_END=$(date +%s%3N)
    STEP_DURATION=$(awk "BEGIN {printf \"%.2f\", ($STEP_END - $STEP_START) / 1000}")
    section_pipe
    section_line "$CHECK" "CMake complete" "${STEP_DURATION}s" 18
fi
section_pipe

# ============================================
# NINJA BUILD PHASE
# ============================================

STEP_START=$(date +%s%3N)

# Resolve wildcards and prepare target list
RESOLVED_TARGETS=()
if [ ${#BUILD_TARGETS[@]} -gt 0 ]; then
    for pattern in "${BUILD_TARGETS[@]}"; do
        if [[ "$pattern" == *"*"* ]]; then
            while IFS= read -r target; do
                RESOLVED_TARGETS+=("$target")
            done < <(ninja -t targets 2>/dev/null | grep -E "^${pattern/\*/.*}" | sed 's/: .*//' | sort -u)
        else
            RESOLVED_TARGETS+=("$pattern")
        fi
    done
fi

if [ ${#RESOLVED_TARGETS[@]} -gt 0 ]; then
    section_line "$HASH" "Ninja" "${RESOLVED_TARGETS[*]} (${BUILD_JOBS} threads)" 18
else
    section_line "$HASH" "Ninja" "all targets (${BUILD_JOBS} threads)" 18
fi
echo ""

# Build with Ninja - unbuffer creates pseudo-TTY so ninja streams in real-time
# Then pipe through formatter for colors (errors=red, warnings=yellow, info=cyan)
# and column wrapping with │ continuation prefix.
export NINJA_STATUS="[%f/%t] %o/s "

NINJA_ARGS=(-j${BUILD_JOBS})
if [ ${#RESOLVED_TARGETS[@]} -gt 0 ]; then
    NINJA_ARGS+=("${RESOLVED_TARGETS[@]}")
fi

# script: pseudo-TTY so ninja streams real-time (stdbuf doesn't work with ninja)
# tr: convert \r to \n (ninja uses \r for TTY progress updates)
# perl: strip ALL ANSI/OSC/charset escape sequences reliably
# stty cols 10000: PTY-width very wide so ninja never wraps internally
script -qfec "stty cols 10000 2>/dev/null; $BUILD_PREFIX ninja ${NINJA_ARGS[*]}" /dev/null 2>&1 \
    | stdbuf -oL tr '\r' '\n' \
    | perl -pe 'BEGIN{$|=1} s/\e\[[0-9;?]*[a-zA-Z]//g; s/\e\][^\a]*\a//g; s/\e.//g' \
    | LC_ALL=C.UTF-8 stdbuf -oL tr -cd '[:print:]\n' \
    | {
    # Spinner state: braille spinner between progress steps (matches section_spin pattern)
    _spin_chr='⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏'
    _spin_i=0
    _spin_lbl="Building"
    _prev_ctx=""
    _max=$((TERM_WIDTH - 7))

    while true; do
        if IFS= read -r -t 0.08 line; then
            # === LINE ARRIVED ===

            # Skip empty/whitespace-only lines (don't touch spinner)
            [[ "$line" =~ ^[[:space:]]*$ ]] && continue

            # gen_log_cat.py pre-formatted output: pass through directly (has ┌─, │ ✓, │ ✗)
            if [[ "$line" == *"┌─"* ]] || [[ "$line" == *"│ ✓"* ]] || [[ "$line" == *"│ ✗"* ]]; then
                [ -n "$_spin_lbl" ] && printf "\r\033[K" && _spin_lbl=""
                printf "%s\033[K\n" "$line"
                continue
            fi

            case "$line" in
                "ninja: no work to do.")
                    [ -n "$_spin_lbl" ] && printf "\r\033[K" && _spin_lbl=""
                    section_line "$CHECK" "Ninja" "no work to do" 18 ;;
                "ninja: build stopped"*)
                    [ -n "$_spin_lbl" ] && printf "\r\033[K" && _spin_lbl=""
                    printf "\n"
                    section_line "$CROSS" "Build stopped" "interrupted" 18 ;;
                "ninja: "*)
                    [ -n "$_spin_lbl" ] && printf "\r\033[K" && _spin_lbl=""
                    section_line "!" "${line#ninja: }" ;;
                *"error:"*|*"Error:"*|*"Fehler:"*|*"fehler:"*|*"FAILED:"*|*"FAILED "*)
                    [ -n "$_spin_lbl" ] && printf "\r\033[K" && _spin_lbl=""
                    _err_file=""
                    if [[ "$line" =~ /([^/]+)\.(cpp|hpp|c|h):([0-9]+) ]]; then
                        _err_file="${BASH_REMATCH[1]}:${BASH_REMATCH[3]}"
                    fi
                    _err_msg=""
                    if [[ "$line" =~ [Ee]rror:\ *(.*) ]]; then
                        _err_msg="${BASH_REMATCH[1]}"
                    elif [[ "$line" =~ [Ff]ehler:\ *(.*) ]]; then
                        _err_msg="${BASH_REMATCH[1]}"
                    fi
                    _col3=$((_max - 44))
                    [ "$_col3" -lt 10 ] && _col3=10
                    if [ -n "$_err_file" ] && [ -n "$_err_msg" ]; then
                        printf "\r  ${SEC_COLOR}│${NC} ${RED}${CROSS}${NC}  ${RED}%-32s${NC} ${MUTED}%-10s${NC} ${TEXT}%s${NC}\033[K\n" \
                            "${_err_file:0:32}" "Fehler" "${_err_msg:0:$_col3}"
                    elif [[ "$line" == *"FAILED"* ]]; then
                        _fail_mod=""
                        _fail_file=""
                        if [[ "$line" =~ /(ase-[a-z0-9-]+)/.*/([^/[:space:]]+) ]]; then
                            _fail_mod="${BASH_REMATCH[1]}"
                            _fail_file="${BASH_REMATCH[2]}"
                        fi
                        if [ -n "$_fail_mod" ]; then
                            printf "\r  ${SEC_COLOR}│${NC} ${RED}${CROSS}${NC}  ${RED}%-20s${NC} ${RED}%-20s${NC} ${TEXT}%s${NC}\033[K\n" \
                                "FAILED" "$_fail_mod" "${_fail_file:0:$_col3}"
                        else
                            printf "\r  ${SEC_COLOR}│${NC} ${RED}${CROSS}${NC}  ${RED}%-20s${NC} ${TEXT}%s${NC}\033[K\n" \
                                "FAILED" "${line:0:$((_max - 22))}"
                        fi
                    else
                        printf "\r  ${SEC_COLOR}│${NC} ${RED}${CROSS}${NC}  ${RED}%s${NC}\033[K\n" "${line:0:$_max}"
                    fi ;;
                *"warning:"*|*"Warning:"*|*"Warnung:"*|*"warnung:"*)
                    [ -n "$_spin_lbl" ] && printf "\r\033[K" && _spin_lbl=""
                    _warn_file=""
                    if [[ "$line" =~ /([^/]+)\.(cpp|hpp|c|h):([0-9]+) ]]; then
                        _warn_file="${BASH_REMATCH[1]}:${BASH_REMATCH[3]}"
                    fi
                    _warn_flag=""
                    if [[ "$line" =~ \[(-W[a-zA-Z0-9_-]+)\] ]]; then
                        _warn_flag="${BASH_REMATCH[1]}"
                    fi
                    _col3=$((_max - 44))
                    [ "$_col3" -lt 10 ] && _col3=10
                    if [ -n "$_warn_file" ]; then
                        printf "\r  ${SEC_COLOR}│${NC} ${YELLOW}!${NC}  ${YELLOW}%-32s${NC} ${MUTED}%-10s${NC} ${MUTED}%s${NC}\033[K\n" \
                            "${_warn_file:0:32}" "Warnung" "${_warn_flag:0:$_col3}"
                    else
                        printf "\r  ${SEC_COLOR}│${NC} ${YELLOW}!${NC}  ${YELLOW}%s${NC}\033[K\n" "${line:0:$_max}"
                    fi ;;
                *"note:"*|*"Hinweis:"*|*"Anmerkung:"*)
                    [ -n "$_spin_lbl" ] && printf "\r\033[K" && _spin_lbl=""
                    _note_file=""
                    if [[ "$line" =~ /([^/]+)\.(cpp|hpp|c|h):([0-9]+) ]]; then
                        _note_file="${BASH_REMATCH[1]}:${BASH_REMATCH[3]}"
                    fi
                    if [ -n "$_note_file" ]; then
                        printf "\r  ${SEC_COLOR}│${NC} ${MUTED}·${NC}  ${MUTED}%-32s${NC} ${MUTED}%-10s${NC}\033[K\n" \
                            "${_note_file:0:32}" "Hinweis"
                    fi ;;
                *)
                    # Filter out compiler diagnostic noise (don't touch spinner)
                    if [[ "$line" =~ ^[[:space:]]*[0-9]+[[:space:]]*\| ]] || \
                       [[ "$line" =~ ^[[:space:]]*\|[[:space:]]*([\^~]) ]] || \
                       [[ "$line" =~ "In member function"|"In Elementfunktion"|"In constructor"|"In Konstruktor" ]] || \
                       [[ "$line" =~ "In file included from"|"In Datei"|"eingefügt von" ]] || \
                       [[ "$line" =~ ^[[:space:]]*/usr/(bin|lib)/ ]] || \
                       [[ "$line" =~ "-std=c++"|"-std=gnu++" ]] || \
                       [[ "$line" =~ "required from"|"erforderlich von"|"instantiated from" ]]; then
                        continue
                    fi

                    # Clear spinner for progress output
                    [ -n "$_spin_lbl" ] && printf "\r\033[K"

                    # Progress lines: zero-padded counter, leading zeros MUTED, digits TEXT
                    if [[ "$line" =~ ^\[([0-9]+/[0-9]+)\]\ ([0-9.]+/s)\ [A-Za-z]+.*/(ase-[a-z0-9-]+)/.*/([^/[:space:]]+) ]]; then
                        _cnt="${BASH_REMATCH[1]}"
                        _rate="${BASH_REMATCH[2]}"
                        _mod="${BASH_REMATCH[3]}"
                        _file="${BASH_REMATCH[4]}"
                        _file="${_file%.cpp.o}"
                        _file="${_file%.cc.o}"
                        _file="${_file%.c.o}"
                        _file="${_file%.o}"
                        IFS='/' read -r _fin _tot <<< "$_cnt"
                        printf -v _pf "%04d" "$((10#$_fin))"; printf -v _pt "%04d" "$((10#$_tot))"
                        _zf="${_pf%%[1-9]*}"; _df="${_pf#"$_zf"}"; [ -z "$_df" ] && _df="0" && _zf="${_pf:0:3}"
                        _zt="${_pt%%[1-9]*}"; _dt="${_pt#"$_zt"}"; [ -z "$_dt" ] && _dt="0" && _zt="${_pt:0:3}"
                        printf -v _fmtcnt "${MUTED}[%s${NC}${TEXT}%s${NC}${MUTED}/%s${NC}${TEXT}%s${NC}${MUTED}]${NC} ${MUTED}%6s${NC}" \
                            "$_zf" "$_df" "$_zt" "$_dt" "$_rate"
                        if [[ "$_mod" != "${_prev_ctx:-}" && -n "${_prev_ctx:-}" ]]; then
                            printf "\r\033[K\n"
                        fi
                        _prev_ctx="$_mod"
                        printf "\r  ${SEC_COLOR}│${NC} ${CYAN}#${NC}  %s  ${CYAN}%-20s${NC} ${TEXT}%s${NC}\033[K\n" \
                            "$_fmtcnt" "$_mod" "$_file"
                        _spin_lbl="$_mod"
                    elif [[ "$line" =~ ^\[([0-9]+/[0-9]+)\]\ ([0-9.]+/s)\ Linking.*/(lib[^/[:space:]]+) ]]; then
                        _cnt="${BASH_REMATCH[1]}"
                        _rate="${BASH_REMATCH[2]}"
                        _lib="${BASH_REMATCH[3]}"
                        IFS='/' read -r _fin _tot <<< "$_cnt"
                        printf -v _pf "%04d" "$((10#$_fin))"; printf -v _pt "%04d" "$((10#$_tot))"
                        _zf="${_pf%%[1-9]*}"; _df="${_pf#"$_zf"}"; [ -z "$_df" ] && _df="0" && _zf="${_pf:0:3}"
                        _zt="${_pt%%[1-9]*}"; _dt="${_pt#"$_zt"}"; [ -z "$_dt" ] && _dt="0" && _zt="${_pt:0:3}"
                        printf -v _fmtcnt "${MUTED}[%s${NC}${TEXT}%s${NC}${MUTED}/%s${NC}${TEXT}%s${NC}${MUTED}]${NC} ${MUTED}%6s${NC}" \
                            "$_zf" "$_df" "$_zt" "$_dt" "$_rate"
                        if [[ "$_lib" != "${_prev_ctx:-}" && -n "${_prev_ctx:-}" ]]; then
                            printf "\r\033[K\n"
                        fi
                        _prev_ctx="$_lib"
                        printf "\r  ${SEC_COLOR}│${NC} ${GREEN}${CHECK}${NC}  %s  ${PURPLE}%s${NC}\033[K\n" \
                            "$_fmtcnt" "$_lib"
                        _spin_lbl="$_lib"
                    elif [[ "$line" =~ ^\[([0-9]+/[0-9]+)\]\ ([0-9.]+/s)\ Linking.*bin/([^/[:space:]]+) ]]; then
                        _cnt="${BASH_REMATCH[1]}"
                        _rate="${BASH_REMATCH[2]}"
                        _exe="${BASH_REMATCH[3]}"
                        IFS='/' read -r _fin _tot <<< "$_cnt"
                        printf -v _pf "%04d" "$((10#$_fin))"; printf -v _pt "%04d" "$((10#$_tot))"
                        _zf="${_pf%%[1-9]*}"; _df="${_pf#"$_zf"}"; [ -z "$_df" ] && _df="0" && _zf="${_pf:0:3}"
                        _zt="${_pt%%[1-9]*}"; _dt="${_pt#"$_zt"}"; [ -z "$_dt" ] && _dt="0" && _zt="${_pt:0:3}"
                        printf -v _fmtcnt "${MUTED}[%s${NC}${TEXT}%s${NC}${MUTED}/%s${NC}${TEXT}%s${NC}${MUTED}]${NC} ${MUTED}%6s${NC}" \
                            "$_zf" "$_df" "$_zt" "$_dt" "$_rate"
                        if [[ "__link_bin__" != "${_prev_ctx:-}" && -n "${_prev_ctx:-}" ]]; then
                            printf "\r\033[K\n"
                        fi
                        _prev_ctx="__link_bin__"
                        printf "\r  ${SEC_COLOR}│${NC} ${GREEN}${CHECK}${NC}  %s  ${PINK}%s${NC}\033[K\n" \
                            "$_fmtcnt" "$_exe"
                        _spin_lbl="$_exe"
                    elif [[ "$line" =~ ^\[([0-9]+/[0-9]+)\]\ ([0-9.]+/s)\ (.+) ]]; then
                        _cnt="${BASH_REMATCH[1]}"
                        _rate="${BASH_REMATCH[2]}"
                        _desc="${BASH_REMATCH[3]}"
                        _action="${_desc%% *}"
                        _target=""
                        if [[ "$_desc" =~ /([^/[:space:]]+)$ ]]; then
                            _target="${BASH_REMATCH[1]}"
                        else
                            _target="${_desc#* }"
                            _target="${_target:0:40}"
                        fi
                        IFS='/' read -r _fin _tot <<< "$_cnt"
                        printf -v _pf "%04d" "$((10#$_fin))"; printf -v _pt "%04d" "$((10#$_tot))"
                        _zf="${_pf%%[1-9]*}"; _df="${_pf#"$_zf"}"; [ -z "$_df" ] && _df="0" && _zf="${_pf:0:3}"
                        _zt="${_pt%%[1-9]*}"; _dt="${_pt#"$_zt"}"; [ -z "$_dt" ] && _dt="0" && _zt="${_pt:0:3}"
                        printf -v _fmtcnt "${MUTED}[%s${NC}${TEXT}%s${NC}${MUTED}/%s${NC}${TEXT}%s${NC}${MUTED}]${NC} ${MUTED}%6s${NC}" \
                            "$_zf" "$_df" "$_zt" "$_dt" "$_rate"
                        _other_ctx="${_action}:${_target}"
                        if [[ "$_other_ctx" != "${_prev_ctx:-}" && -n "${_prev_ctx:-}" ]]; then
                            printf "\r\033[K\n"
                        fi
                        _prev_ctx="$_other_ctx"
                        printf "\r  ${SEC_COLOR}│${NC} ${MUTED}#${NC}  %s  ${TEXT}%-20s${NC} ${MUTED}%s${NC}\033[K\n" \
                            "$_fmtcnt" "$_action" "$_target"
                        _spin_lbl="$_action"
                    else
                        # Non-progress lines without [counter] prefix: suppress
                        _spin_lbl=""
                        continue
                    fi ;;
            esac
        else
            # === TIMEOUT OR EOF ===
            _rc=$?
            if [ $_rc -gt 128 ]; then
                # Timeout: draw spinner frame while waiting for next step
                if [ -n "$_spin_lbl" ]; then
                    printf "\r  ${SEC_COLOR}│${NC} ${MUTED}${_spin_chr:$_spin_i:1}${NC}  ${MUTED}%s${NC}\033[K" "$_spin_lbl"
                    _spin_i=$(( (_spin_i + 1) % 10 ))
                fi
            else
                # EOF: pipe closed
                break
            fi
        fi
    done

    # Cleanup: clear any remaining spinner
    [ -n "$_spin_lbl" ] && printf "\r\033[K"
}
NINJA_EXIT=${PIPESTATUS[0]}

if [ $NINJA_EXIT -ne 0 ]; then
    echo ""
    section_line "$CROSS" "Build failed!"
    exit 1
fi

STEP_END=$(date +%s%3N)
STEP_DURATION=$(awk "BEGIN {printf \"%.2f\", ($STEP_END - $STEP_START) / 1000}")
echo ""
section_line "$CHECK" "Build complete" "${STEP_DURATION}s" 18

# ============================================
# BUILD SUMMARY
# ============================================

# Count built artifacts
BIN_COUNT=$(find bin -type f -executable 2>/dev/null | wc -l)
LIB_COUNT=$(find lib -name "*.a" -o -name "*.so" 2>/dev/null | wc -l)

# Bump build number
NEW_BUILD=$(bump_build)

# Calculate build duration
BUILD_END_TIME=$(date +%s)
BUILD_DURATION=$((BUILD_END_TIME - BUILD_START_TIME))
BUILD_MINUTES=$((BUILD_DURATION / 60))
BUILD_SECONDS=$((BUILD_DURATION % 60))

section_header "Summary" 179

section_line "@" "Duration" "${BUILD_MINUTES}m ${BUILD_SECONDS}s" 18
section_line "$HASH" "Artifacts" "${BIN_COUNT} binaries, ${LIB_COUNT} libraries" 18
section_line "*" "Version" "v${VERSION} [${STATUS}]" 18
section_line "$HASH" "Build" "#${NEW_BUILD}" 18
section_line "$CHECK" "Status" "Ready" 18

# ============================================
# NEXT STEPS
# ============================================

section_header "Next Steps" 110

section_line "$ARROW" "Run" "./build.sh run"
section_line "$ARROW" "Debug" "./build.sh debug run"
section_line "$ARROW" "Targets" "./build.sh targets"
echo ""

# ============================================
# RUN (if requested)
# ============================================

if [ "$DO_RUN" = true ]; then
    EXPLORER_BIN="$BUILD_DIR/bin/ase-explorer"
    if [ ! -x "$EXPLORER_BIN" ]; then
        section_line "$CROSS" "Binary not found" "$EXPLORER_BIN"
        exit 1
    fi
    section_header "Run" 71
    section_line "$ARROW" "Starting ase-explorer"
    echo ""
    "$EXPLORER_BIN" "${RUN_ARGS[@]}"
fi
