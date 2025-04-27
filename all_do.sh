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
##   -t  | --test          Run the unit tests
##   -c  | --clean         Delete build/ directory only
##   -w  | --wipe          Delete build and compile commands, keep CMake-related files
##   -n  | --nuke-all      Full nuke (build/, .sln, .vcxproj, CMakeLists.txt.user)
##   -cc | --compile-db    Only regenerate compile_commands.json into buildfiles/
##   -h  | --help          Show this help message

set -euo pipefail

# ====== Configuration ======
BUILD_DIR="build"
BUILD_TYPE="Debug"
BUILD_PRESET_WINDOWS="windows-dynamic"
BUILD_PRESET_LINUX="linux-dynamic"
LOG_FILE="logs/build.log"
COMPILE_DB_OUT="buildfiles"

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
    --test|-t) DO_TEST=true ;;
    --compile-db|-cc) DO_COMPILE_DB=true ;;
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

function wipe_build_artifacts() {
  if $NUKE_ALL; then
    section "Nuking ALL build artifacts"
    echo -e "${RED}WARNING: Deleting all build artifacts!${NC}"
    rm -rf "$BUILD_DIR" *.sln *.vcxproj* .gitlab-ci.yml CMakeLists.txt.user
  elif $DO_WIPE; then
    section "Wiping build directory"
    echo -e "${RED}WARNING: Deleting build directory!${NC}"
    rm -rf "$BUILD_DIR"
  elif $DO_CLEAN; then
    section "Cleaning cache and CMake temp files"
    echo -e "${RED}WARNING: Deleting CMake cache and temp files!${NC}"
    rm -f "$BUILD_DIR"/CMakeCache.txt "$BUILD_DIR"/cmake_install.cmake
  fi
}

function compile_db_only() {
  section "Exporting compile_commands.json"
  cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -B "$BUILD_DIR" -G Ninja
  cp -f "$BUILD_DIR/compile_commands.json" "$COMPILE_DB_OUT/"
  echo "Compile commands exported to: $COMPILE_DB_OUT/compile_commands.json"
}

# ====== Compile DB step only ======
if $DO_COMPILE_DB; then
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

# ====== Build Section ======
if $DO_BUILD; then
  wipe_build_artifacts
  
  CURRENT_STEP="Configuring with CMake ($PRESET)"
  section "$CURRENT_STEP"
  cmake --preset "$PRESET"

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
  EXECUTABLE_PATH="$BUILD_DIR/$PRESET/bin/windows/KillApiconnect.exe"
  gdb -x gdbinit "$EXECUTABLE_PATH"
fi

# ====== Finalize ======
CURRENT_STEP="Finalizing"
section "$CURRENT_STEP"
rundll32 user32.dll,MessageBeep || true
echo -e "${GREEN}All steps completed successfully!${NC}"
echo "Log file: $LOG_FILE"
