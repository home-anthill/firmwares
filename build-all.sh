#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

FIRMWARES=(
  airquality-pir
  barometer
  dht-light
  ac-beko
  ac-lg
  thermostat
)

BOARDS=(
  esp32s2
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

prepare_secrets() {
  local firmware="$1"
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
  prepare_secrets "$firmware"
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

    if [[ "$clean_tests" == true ]]; then
      rm -rf "$ROOT_DIR/$firmware/tests/build"
    fi

    cmake -S "$ROOT_DIR/$firmware/tests" -B "$ROOT_DIR/$firmware/tests/build"
    cmake --build "$ROOT_DIR/$firmware/tests/build" --parallel
    ctest --test-dir "$ROOT_DIR/$firmware/tests/build" --output-on-failure
  done
fi
