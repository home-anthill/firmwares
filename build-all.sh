#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

FIRMWARES=(
  motion
  airquality-airpressure
  lux-temp-hum
  pm
  ph
  water-level
  ac-beko
  ac-lg
  thermostat
  thermocouple-mcp9601
)

BOARDS=(
  esp32s3
)

usage() {
  cat <<EOF
Usage: $(basename "$0") [options]

Build all Arduino firmwares and run all host unit tests.

Options:
  --build-only       Build firmwares, skip unit tests
  --test-only        Run unit tests, skip firmware builds
  --clean-tests      Remove each tests/build directory before configuring tests
  -h, --help         Show this help
EOF
}

build_firmwares=true
run_tests=true
clean_tests=false

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-only)
      run_tests=false
      ;;
    --test-only)
      build_firmwares=false
      ;;
    --clean-tests)
      clean_tests=true
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
  shift
done

require_command() {
  local command_name="$1"

  if ! command -v "$command_name" >/dev/null 2>&1; then
    echo "Missing required command: $command_name" >&2
    exit 1
  fi
}

prepare_headers() {
  local firmware="$1"
  local config_path="$ROOT_DIR/$firmware/config.h"
  if [[ ! -f "$config_path" ]]; then
    cp "$ROOT_DIR/config-$firmware-template" "$config_path"
  fi
  local secrets_path="$ROOT_DIR/$firmware/secrets.h"

  if [[ ! -f "$secrets_path" ]]; then
    cp "$ROOT_DIR/secrets-template" "$secrets_path"
  fi
}

section() {
  echo
  echo "==> $*"
}

for firmware in "${FIRMWARES[@]}"; do
  prepare_headers "$firmware"
done

if [[ "$build_firmwares" == true ]]; then
  require_command arduino-cli

  for board in "${BOARDS[@]}"; do
    section "Building firmwares for $board"

    for firmware in "${FIRMWARES[@]}"; do
      section "Building $firmware for $board"
      (
        cd "$ROOT_DIR/$firmware"
        arduino-cli compile --fqbn "esp32:esp32:$board" "./$firmware.ino"
      )
    done
  done
fi

if [[ "$run_tests" == true ]]; then
  require_command cmake
  require_command ctest

  for firmware in "${FIRMWARES[@]}"; do
    section "Running tests for $firmware"

    test_source_dir="$ROOT_DIR/$firmware/tests"
    test_build_dir="$test_source_dir/build"
    cache_path="$test_build_dir/CMakeCache.txt"
    stale_cache=false

    if [[ -f "$cache_path" ]]; then
      cached_source_dir="$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$cache_path")"
      cached_build_dir="$(sed -n 's/^CMAKE_CACHEFILE_DIR:INTERNAL=//p' "$cache_path")"
      if [[ "$cached_source_dir" != "$test_source_dir" || "$cached_build_dir" != "$test_build_dir" ]]; then
        stale_cache=true
        section "Resetting stale test build directory for $firmware"
      fi
    fi

    if [[ "$clean_tests" == true || "$stale_cache" == true ]]; then
      # FetchContent sub-build caches also contain absolute paths, so reset
      # the entire generated directory when a firmware or checkout is moved.
      rm -rf "$test_build_dir"
    fi

    cmake -S "$test_source_dir" -B "$test_build_dir"
    cmake --build "$test_build_dir" --parallel
    ctest --test-dir "$test_build_dir" --output-on-failure
  done
fi
