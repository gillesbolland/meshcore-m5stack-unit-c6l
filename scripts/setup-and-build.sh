#!/usr/bin/env bash
# Clone pinned MeshCore, apply this overlay + patches, build hybrid companion into firmware/.
set -eu

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
MESHCORE_TAG="$(python3 -c "import json; print(json.load(open('${REPO_ROOT}/COMPAT.json'))['meshcore_tag'])")"
BUILD_DIR="${BUILD_DIR:-${REPO_ROOT}/.build/MeshCore}"
MESHCORE_URL="https://github.com/meshcore-dev/MeshCore.git"

# Hybrid companion (BLE + USB + WiFi Mode) — only role this overlay ships
COMPANION_ENV=m5stack_unit_c6l_companion_radio_ble

hint_missing() {
  local tool="$1"
  echo ""
  echo "Missing dependency: $tool"
  echo "Install it, then re-run:"
  echo ""
  case "$(uname -s)" in
    Darwin)
      case "$tool" in
        git) echo "  brew install git" ;;
        python3) echo "  brew install python" ;;
        pio|platformio)
          echo "  brew install platformio"
          echo "  # or:  pipx install platformio"
          ;;
        *) echo "  brew install $tool" ;;
      esac
      ;;
    Linux)
      if command -v apt-get >/dev/null 2>&1; then
        case "$tool" in
          git) echo "  sudo apt update && sudo apt install -y git" ;;
          python3) echo "  sudo apt update && sudo apt install -y python3" ;;
          pio|platformio)
            echo "  pipx install platformio"
            echo "  # or see https://platformio.org/install/cli"
            ;;
          *) echo "  sudo apt install -y $tool" ;;
        esac
      else
        echo "  # install $tool via your package manager or pipx"
      fi
      ;;
    *)
      echo "  # install $tool for your OS"
      ;;
  esac
  echo ""
}

need() {
  local cmd="$1"
  local label="${2:-$1}"
  if ! command -v "$cmd" >/dev/null 2>&1; then
    hint_missing "$label"
    exit 1
  fi
}

need git
need python3
if ! command -v pio >/dev/null 2>&1; then
  hint_missing platformio
  exit 1
fi

# Warn about network / synced volumes (SCons .sconsign issues)
case "$BUILD_DIR" in
  /Volumes/*)
    echo "Note: build dir is under /Volumes — if SCons fails on .sconsign, set BUILD_DIR to a local APFS path."
    ;;
esac

FIRMWARE_VERSION="${FIRMWARE_VERSION:-v${MESHCORE_TAG#companion-v}-overlay}"
OVERLAY_HASH="$(cd "$REPO_ROOT" && git rev-parse --short HEAD 2>/dev/null || echo local)"
FIRMWARE_VERSION_STRING="${FIRMWARE_VERSION}-${OVERLAY_HASH}"
FIRMWARE_BUILD_DATE="$(date '+%d-%b-%Y')"

echo "==> MeshCore pin: ${MESHCORE_TAG}"
echo "==> Build dir:   ${BUILD_DIR}"
echo "==> Version:     ${FIRMWARE_VERSION_STRING}"

if [ ! -d "${BUILD_DIR}/.git" ]; then
  mkdir -p "$(dirname "$BUILD_DIR")"
  rm -rf "$BUILD_DIR"
  git clone --depth 1 --branch "$MESHCORE_TAG" "$MESHCORE_URL" "$BUILD_DIR"
else
  cd "$BUILD_DIR"
  git fetch --depth 1 origin "refs/tags/${MESHCORE_TAG}:refs/tags/${MESHCORE_TAG}" 2>/dev/null || true
  git checkout -f "$MESHCORE_TAG"
  git clean -fdx -e .pio
fi

cd "$BUILD_DIR"

echo "==> Overlaying variant (rsync --delete)..."
mkdir -p variants
rsync -a --delete "${REPO_ROOT}/overlay/variants/m5stack_unit_c6l/" \
  "variants/m5stack_unit_c6l/"

echo "==> Copying SSD1306SPIDisplay..."
mkdir -p src/helpers/ui
cp "${REPO_ROOT}/overlay/src/helpers/ui/SSD1306SPIDisplay.h" \
   "${REPO_ROOT}/overlay/src/helpers/ui/SSD1306SPIDisplay.cpp" \
   src/helpers/ui/

echo "==> Applying patches..."
# Reset tracked files that patches touch, then apply (idempotent rebuild)
git checkout -f -- \
  examples/companion_radio/MyMesh.h \
  examples/companion_radio/MyMesh.cpp \
  examples/companion_radio/AbstractUITask.h \
  examples/companion_radio/main.cpp \
  src/helpers/esp32/SerialBLEInterface.cpp \
  src/helpers/MultiSerialInterface.h \
  src/MeshCore.h \
  src/helpers/ESP32Board.h \
  src/helpers/radiolib/RadioLibWrappers.cpp \
  2>/dev/null || true

shopt -s nullglob
for p in "${REPO_ROOT}/patches/"*.patch; do
  echo "  apply $(basename "$p")"
  if ! git apply --check "$p"; then
    echo "ERROR: patch does not apply to ${MESHCORE_TAG}: $p"
    echo "Pin may be broken for this overlay. Do not hand-edit MeshCore silently."
    exit 1
  fi
  git apply "$p"
done
shopt -u nullglob

mkdir -p "${REPO_ROOT}/firmware" out
USER_PLATFORMIO_BUILD_FLAGS="${PLATFORMIO_BUILD_FLAGS:-}"

# Public artifact version (MeshCore-aligned), e.g. v1.17.1
PUB_VER="${FIRMWARE_VERSION:-v${MESHCORE_TAG#companion-v}}"
case "$PUB_VER" in
  v*) ;;
  *) PUB_VER="v${PUB_VER}" ;;
esac

echo "---- Building hybrid companion (${COMPANION_ENV}) ----"
export PLATFORMIO_BUILD_FLAGS="${USER_PLATFORMIO_BUILD_FLAGS}"
export PLATFORMIO_BUILD_FLAGS="${PLATFORMIO_BUILD_FLAGS} -DFIRMWARE_BUILD_DATE='\"${FIRMWARE_BUILD_DATE}\"' -DFIRMWARE_VERSION='\"${FIRMWARE_VERSION_STRING}\"'"
pio run -e "${COMPANION_ENV}" -j 1
pio run -t mergebin -e "${COMPANION_ENV}" -j 1

# Remove previous bins so only companion remains
rm -f "${REPO_ROOT}/firmware/"*.bin
APP_OUT="${REPO_ROOT}/firmware/m5stack_unit_c6l_companion-${PUB_VER}.bin"
MERGED_OUT="${REPO_ROOT}/firmware/m5stack_unit_c6l_companion-${PUB_VER}-merged.bin"
cp ".pio/build/${COMPANION_ENV}/firmware.bin" "$APP_OUT"
cp ".pio/build/${COMPANION_ENV}/firmware-merged.bin" "$MERGED_OUT"
cp "$APP_OUT" "out/$(basename "$APP_OUT")"
cp "$MERGED_OUT" "out/$(basename "$MERGED_OUT")"

cat > "${REPO_ROOT}/firmware/README.md" <<EOF
# Prebuilt firmware

Hybrid **companion** only (BLE + USB + WiFi Mode).

- **MeshCore pin:** \`${MESHCORE_TAG}\`
- **Build stamp:** \`${FIRMWARE_VERSION_STRING}\`
- **Build date:** ${FIRMWARE_BUILD_DATE}

| File | Use |
|------|-----|
| \`m5stack_unit_c6l_companion-${PUB_VER}.bin\` | App update @ \`0x10000\` |
| \`m5stack_unit_c6l_companion-${PUB_VER}-merged.bin\` | Full flash @ \`0x0\` |

Flash with \`../scripts/flash.sh\`.

**WiFi:** this prebuilt has no baked-in SSID. Rebuild with \`-D WIFI_SSID=...\` / \`-D WIFI_PWD=...\` (see docs).
EOF

echo ""
echo "Artifacts written to ${REPO_ROOT}/firmware/"
ls -la "${REPO_ROOT}/firmware/"*.bin
