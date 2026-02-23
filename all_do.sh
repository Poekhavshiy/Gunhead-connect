#!/usr/bin/env bash
## all_do.sh - Enhanced build orchestrator for Qt/CMake projects
##
## Usage:
##   ./all_do.sh [option]
##
## Options:
##   -arrr | --release           Build in release mode
##   -br | --build-run     Build, Test and then run the executable
##   -jb | --just-build    Only build the project
##   -r | --run            Run the built executable through gdb
##   -d | --debug         Enable debug mode
##   -c | --console       Enable debugging to console mode
##   -t  | --test          Run the unit tests
##   -c  | --clean         Delete build/ directory only
##   -w  | --wipe          Delete build and compile commands, keep CMake-related files
##   -n  | --nuke-all      Full nuke (build/, .sln, .vcxproj, CMakeLists.txt.user)
##   -cc | --compile-db    Only regenerate compile_commands.json into buildfiles/
##   --diag | --diagnose   Enable CMake trace/debug find for troubleshooting
##   --no-fetch            Disable FetchContent (requires system packages installed)
##   --no-tests            Disable building tests (skips Catch2)
##   -h  | --help          Show this help message

set -euo pipefail

# ====== Configuration ======
BUILD_DIR="build"
BUILD_TYPE="Debug"
BUILD_PRESET_WINDOWS="windows-dynamic"
BUILD_PRESET_LINUX="linux-dynamic"
LOG_FILE="logs/build.log"
COMPILE_DB_OUT="devtools/buildfiles"

# ====== Color output ======
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# ====== Initialize log ======
mkdir -p data "$COMPILE_DB_OUT"
#echo "" > "$LOG_FILE"
exec > >(tee -a "$LOG_FILE") 2>&1

# Timestamp function
timestamp() { date '+[%Y-%m-%d %H:%M:%S]'; }

# Trap any error and report
trap 'echo -e "\n${RED}[ERROR]$(timestamp) Step failed: $CURRENT_STEP${NC}" >&2; rundll32 user32.dll,MessageBeep; sleep 1; rundll32 user32.dll,MessageBeep; sleep 1; rundll32 user32.dll,MessageBeep; exit 1' ERR

# Clear terminal
clear

# ====== Default flags ======
NUKE_ALL=false
DO_BUILD=false
DO_RUN=false
DO_HELP=false
DO_CLEAN=false
DO_WIPE=false
DO_COMPILE_DB=false
DO_TEST=false
EXTRA_RUN_ARGS=""
DIAGNOSE=false
CMAKE_DEBUG_ARGS=""
CMAKE_EXTRA_ARGS=${CMAKE_EXTRA_ARGS:-}
OFFLINE=false
NO_FETCH=false
FETCH_DEPS_FLAG="-DFETCH_DEPS=ON"
ENABLE_TESTS_FLAG="-DENABLE_TESTS=ON"

# ====== Parse CLI arguments ======
for arg in "$@"; do
  case "$arg" in
    --release|-arrr) BUILD_TYPE="Release"; NUKE_ALL=true; DO_BUILD=true; DO_RUN=true ;; 
    --nuke-all|-n) NUKE_ALL=true ;;
    --wipe) DO_WIPE=true ;;
    --clean) DO_CLEAN=true ;;
    --build-run|-br) DO_BUILD=true; DO_TEST=true; DO_RUN=true ;;
    --just-build|-jb) DO_BUILD=true ;;
    --run|-r) DO_RUN=true ;;
    --debug|-d) EXTRA_RUN_ARGS="$EXTRA_RUN_ARGS --debug" ;;
    --console|-c) EXTRA_RUN_ARGS="$EXTRA_RUN_ARGS --console" ;;
    --test|-t) DO_TEST=true ;;
    --compile-db|-cc) DO_COMPILE_DB=true ;;
  --offline) OFFLINE=true ;;
  --no-fetch) NO_FETCH=true ;;
  --no-tests) ENABLE_TESTS_FLAG="-DENABLE_TESTS=OFF" ;;
    --diag|--diagnose) DIAGNOSE=true ;;
    --help|-h) DO_HELP=true ;;
    *) echo -e "${RED}Unknown option: $arg${NC}" >&2; exit 1 ;;
  esac
done

# ====== Documentation ======
function show_help() {
  # Print help
  grep '^## ' "$0" | cut -c 4-
  exit 0
}

if [ "$#" -eq 0 ] || $DO_HELP; then
  show_help
  exit 0
fi

# ====== Utility Functions ======
function section() {
  echo -e "\n${BLUE}==== $(timestamp) $1 ====${NC}"
}

function sanity_checks() {
  section "Environment sanity checks"
  echo "Shell: $SHELL"
  echo "PWD: $(pwd)"
  echo "uname -s: $(uname -s)"
  echo "Preset: $PRESET | Build type: $BUILD_TYPE"
  echo "cmake version: $(cmake --version 2>/dev/null | head -n1 || echo 'cmake not found')"
  echo "ninja version: $(command -v ninja >/dev/null 2>&1 && ninja --version || echo 'ninja not found')"
  echo "g++ version: $(command -v g++ >/dev/null 2>&1 && g++ --version | head -n1 || echo 'g++ not found')"

  # Validate expected toolchain locations from presets (best-effort)
  if [ -x "/c/msys64/mingw64/bin/g++.exe" ]; then
    echo "Found MinGW64 g++ at /c/msys64/mingw64/bin/g++.exe"
  else
    echo -e "${RED}WARNING:${NC} /c/msys64/mingw64/bin/g++.exe not found. Ensure MSYS2 MinGW 64-bit toolchain is installed."
  fi

  # Validate Qt prefix path used by presets
  if [ -d "/c/Dev/Qt/qt-install-dynamic/lib/cmake/Qt6" ]; then
    echo "Found Qt6 CMake package at /c/Dev/Qt/qt-install-dynamic/lib/cmake/Qt6"
  else
    echo -e "${RED}WARNING:${NC} Qt6_DIR/CMAKE_PREFIX_PATH not found at /c/Dev/Qt/qt-install-dynamic. Update CMakePresets.json or install Qt."
  fi
}

function wipe_build_artifacts() {
  if $NUKE_ALL; then
    section "Nuking ALL build artifacts"
    echo -e "${RED}WARNING: Deleting all build artifacts!${NC}"
    rm -rf "$BUILD_DIR" *.sln *.vcxproj* .gitlab-ci.yml CMakeLists.txt.user .fc_cache
  elif $DO_WIPE; then
    section "Wiping build directory"
    echo -e "${RED}WARNING: Deleting build directory!${NC}"
    rm -rf "$BUILD_DIR" .fc_cache
  elif $DO_CLEAN; then
    section "Cleaning cache and CMake temp files"
    echo -e "${RED}WARNING: Deleting CMake cache and temp files!${NC}"
    rm -f "$BUILD_DIR"/CMakeCache.txt "$BUILD_DIR"/cmake_install.cmake
    # Also remove FetchContent subbuild state to avoid stuck downloads
    find "$BUILD_DIR" -maxdepth 2 -type d -name "_deps" -exec rm -rf {} + || true
    find "$BUILD_DIR" -type d -name "*subbuild" -exec rm -rf {} + || true
  fi
}

# ====== First handle cleaning operations ======
if $NUKE_ALL || $DO_WIPE || $DO_CLEAN; then
  wipe_build_artifacts
  DO_COMPILE_DB=true
fi

function compile_db_only() {
  section "Exporting compile_commands.json"
  cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -B "$BUILD_DIR" -G Ninja
  cp -f "$BUILD_DIR/compile_commands.json" "$COMPILE_DB_OUT/"
  echo "Compile commands exported to: $COMPILE_DB_OUT/compile_commands.json"
}

# ====== Compile DB step only ======
if $DO_COMPILE_DB && ! $DO_BUILD; then
  compile_db_only
  exit 0
fi

# Platform preset detection
  OS_NAME=$(uname -s)
  if [[ "$OS_NAME" =~ MINGW* ]]; then
    PRESET="$BUILD_PRESET_WINDOWS"
  else
    PRESET="$BUILD_PRESET_LINUX"
  fi

# Release build Suffix
if [[ "$BUILD_TYPE" == "Release" ]]; then
  PRESET="${PRESET}-release"
fi

# Enable CMake debug tracing if diagnosing
if $DIAGNOSE; then
  CMAKE_DEBUG_ARGS="--log-level=VERBOSE --debug-find --trace-expand -Wdev"
  set -x
fi

# ====== Build Section ======
if $DO_BUILD; then
  sanity_checks
 # Generate compile_commands.json as part of build process if requested
  if $DO_COMPILE_DB; then
    CURRENT_STEP="Generating compile_commands.json"
    section "$CURRENT_STEP"
  cmake $CMAKE_DEBUG_ARGS $CMAKE_EXTRA_ARGS -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -B "$BUILD_DIR" -G Ninja
    cp -f "$BUILD_DIR/compile_commands.json" "$COMPILE_DB_OUT/"
    echo "Compile commands exported to: $COMPILE_DB_OUT/compile_commands.json"
  fi

  CURRENT_STEP="Configuring with CMake ($PRESET)"
  section "$CURRENT_STEP"
  # Environment for offline and git verbosity
  if $OFFLINE; then
    export FETCHCONTENT_FULLY_DISCONNECTED=ON
  fi
  if $NO_FETCH; then
    FETCH_DEPS_FLAG="-DFETCH_DEPS=OFF"
  fi
  if $DIAGNOSE; then
    export GIT_TRACE=1
    export GIT_TRACE_PACKET=1
    export GIT_CURL_VERBOSE=1
  fi
  cmake $CMAKE_DEBUG_ARGS $CMAKE_EXTRA_ARGS $FETCH_DEPS_FLAG $ENABLE_TESTS_FLAG --preset "$PRESET"

  CURRENT_STEP="Building project ($PRESET)"
  section "$CURRENT_STEP"
  cmake --build --preset "build-$PRESET"
fi

# ====== Test Section ======
if $DO_TEST; then
  CURRENT_STEP="Running tests"
  section "$CURRENT_STEP"
  ctest --preset "test-$PRESET" --output-on-failure
fi

# ====== Run Section ======
if $DO_RUN; then
  CURRENT_STEP="Running executable"
  section "$CURRENT_STEP"
  EXECUTABLE_PATH="$BUILD_DIR/$PRESET/bin/windows/Gunhead-Connect.exe"
  gdb -x gdbinit --args "$EXECUTABLE_PATH" $EXTRA_RUN_ARGS
fi

# ====== Finalize ======
CURRENT_STEP="Finalizing"
section "$CURRENT_STEP"
rundll32 user32.dll,MessageBeep || true
echo -e "${GREEN}All steps completed successfully!${NC}"
echo "Log file: $LOG_FILE"
