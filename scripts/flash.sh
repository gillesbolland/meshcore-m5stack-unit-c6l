#!/usr/bin/env bash
# Flash prebuilt hybrid companion firmware from firmware/ (no PlatformIO required).
# One image: BLE + USB + WiFi Mode (built from m5stack_unit_c6l_companion_radio_ble).
set -eu

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
FIRMWARE_DIR="${REPO_ROOT}/firmware"

CHIP=esp32c6
FLASH_MODE=dio
FLASH_FREQ=80m
FLASH_SIZE=4MB
APP_OFF=0x10000
MERGED_OFF=0x0

hint_esptool_install() {
  echo ""
  echo "esptool not found. Install it, then re-run this script:"
  echo ""
  case "$(uname -s)" in
    Darwin)
      echo "  brew install esptool"
      echo "  # or:  pipx install esptool"
      ;;
    Linux)
      if command -v apt-get >/dev/null 2>&1; then
        echo "  sudo apt update && sudo apt install -y esptool"
        echo "  # or:  pipx install esptool"
      else
        echo "  pipx install esptool"
        echo "  # or:  pip install --user esptool"
      fi
      ;;
    *)
      echo "  pipx install esptool"
      echo "  # or:  pip install --user esptool"
      ;;
  esac
  echo ""
}

find_esptool() {
  if command -v esptool >/dev/null 2>&1; then
    echo esptool
    return 0
  fi
  if command -v esptool.py >/dev/null 2>&1; then
    echo esptool.py
    return 0
  fi
  return 1
}

ESPTOOL_BIN=""
if ! ESPTOOL_BIN="$(find_esptool)"; then
  hint_esptool_install
  exit 1
fi

pick_port() {
  if [ -n "${PORT:-}" ]; then
    echo "$PORT"
    return
  fi
  local ports=()
  if [ "$(uname -s)" = Darwin ]; then
    # shellcheck disable=SC2206
    ports=(/dev/cu.usbmodem* /dev/cu.usbserial* /dev/cu.wchusbserial*)
  else
    # shellcheck disable=SC2206
    ports=(/dev/ttyACM* /dev/ttyUSB*)
  fi
  local found=()
  for p in "${ports[@]}"; do
    [ -e "$p" ] && found+=("$p")
  done
  if [ ${#found[@]} -eq 1 ]; then
    echo "${found[0]}"
    return
  fi
  if [ ${#found[@]} -gt 1 ]; then
    echo "Multiple serial ports found:" >&2
    local i=1
    for p in "${found[@]}"; do
      echo "  [$i] $p" >&2
      i=$((i + 1))
    done
    printf "Select port number: " >&2
    read -r idx
    echo "${found[$((idx - 1))]}"
    return
  fi
  echo "" >&2
  echo "No serial port auto-detected. Set PORT=/dev/... and re-run." >&2
  echo "Download mode: hold Unit C6L Reset ~3s, then connect USB." >&2
  exit 1
}

latest_bin() {
  local kind="$1" # app | merged
  local best=""
  local f
  if [ "$kind" = merged ]; then
    for f in "${FIRMWARE_DIR}"/m5stack_unit_c6l_companion-*-merged.bin; do
      [ -f "$f" ] || continue
      if [ -z "$best" ] || [ "$f" -nt "$best" ]; then best="$f"; fi
    done
  else
    for f in "${FIRMWARE_DIR}"/m5stack_unit_c6l_companion-*.bin; do
      [ -f "$f" ] || continue
      case "$f" in *-merged.bin) continue ;; esac
      if [ -z "$best" ] || [ "$f" -nt "$best" ]; then best="$f"; fi
    done
  fi
  echo "$best"
}

print_download_hint() {
  echo ""
  echo "Unit C6L download mode: hold the side Reset button ~3 seconds until the"
  echo "device enters download mode, then release. Use a USB data cable."
  echo ""
}

usage() {
  cat <<EOF
Usage: $0 [--port PORT] [--update|--full|--erase] [--yes]

Flashes the hybrid companion image (BLE + USB + WiFi Mode).
Environment: PORT=/dev/...
EOF
}

ACTION=""
ASSUME_YES=0
while [ $# -gt 0 ]; do
  case "$1" in
    --port) PORT="$2"; shift 2 ;;
    --update) ACTION=update; shift ;;
    --full) ACTION=full; shift ;;
    --erase) ACTION=erase; shift ;;
    --yes|-y) ASSUME_YES=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown arg: $1"; usage; exit 1 ;;
  esac
done

if [ -z "$ACTION" ]; then
  print_download_hint
  echo "MeshCore Unit C6L companion flash  (esptool=$ESPTOOL_BIN)"
  echo ""
  echo "  [1] Update (app only @ ${APP_OFF})"
  echo "  [2] Full flash (erase + merged @ ${MERGED_OFF})"
  echo "  [3] Erase only"
  echo "  [q] Quit"
  echo ""
  printf "Choice: "
  read -r choice
  case "$choice" in
    1) ACTION=update ;;
    2) ACTION=full ;;
    3) ACTION=erase ;;
    q|Q) exit 0 ;;
    *) echo "Cancelled"; exit 1 ;;
  esac
fi

PORT_RESOLVED="$(pick_port)"
echo "Using port: $PORT_RESOLVED"

COMMON=(--chip "$CHIP" -p "$PORT_RESOLVED")
FLASH_OPTS=(--flash-mode "$FLASH_MODE" --flash-freq "$FLASH_FREQ" --flash-size "$FLASH_SIZE")

run_cmd() {
  echo ""
  echo "+ $*"
  echo ""
  "$@"
}

case "$ACTION" in
  erase)
    CMD=("$ESPTOOL_BIN" "${COMMON[@]}" erase-flash)
    printf '%q ' "${CMD[@]}"; echo
    if [ "$ASSUME_YES" -eq 1 ] || { printf "Erase entire flash? [y/N] "; read -r a; [ "$a" = y ] || [ "$a" = Y ]; }; then
      run_cmd "${CMD[@]}"
    fi
    ;;
  update)
    APP="$(latest_bin app)"
    if [ -z "$APP" ] || [ ! -f "$APP" ]; then
      echo "No companion app firmware under $FIRMWARE_DIR (expected m5stack_unit_c6l_companion-*.bin)"
      exit 1
    fi
    CMD=("$ESPTOOL_BIN" "${COMMON[@]}" write-flash "${FLASH_OPTS[@]}" "$APP_OFF" "$APP")
    printf '%q ' "${CMD[@]}"; echo
    run_cmd "${CMD[@]}"
    ;;
  full)
    MERGED="$(latest_bin merged)"
    if [ -z "$MERGED" ] || [ ! -f "$MERGED" ]; then
      echo "No companion merged firmware under $FIRMWARE_DIR (expected m5stack_unit_c6l_companion-*-merged.bin)"
      exit 1
    fi
    ERASE_CMD=("$ESPTOOL_BIN" "${COMMON[@]}" erase-flash)
    WRITE_CMD=("$ESPTOOL_BIN" "${COMMON[@]}" write-flash "${FLASH_OPTS[@]}" "$MERGED_OFF" "$MERGED")
    printf '%q ' "${ERASE_CMD[@]}"; echo
    printf '%q ' "${WRITE_CMD[@]}"; echo
    if [ "$ASSUME_YES" -eq 1 ] || { printf "Erase then write merged image? [y/N] "; read -r a; [ "$a" = y ] || [ "$a" = Y ]; }; then
      run_cmd "${ERASE_CMD[@]}"
      run_cmd "${WRITE_CMD[@]}"
    fi
    ;;
  *)
    echo "Unknown action: $ACTION"
    exit 1
    ;;
esac

echo "Done."
