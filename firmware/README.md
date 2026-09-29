# Prebuilt firmware

Hybrid **companion** only (BLE + USB + WiFi Mode in one image).

- **MeshCore pin:** `companion-v1.17.1`
- **Version:** `v1.17.1`

| File | Use |
|------|-----|
| `m5stack_unit_c6l_companion-v1.17.1.bin` | App update @ `0x10000` |
| `m5stack_unit_c6l_companion-v1.17.1-merged.bin` | Full flash @ `0x0` |

Flash with [`../scripts/flash.sh`](../scripts/flash.sh).

**WiFi:** this prebuilt has **no** baked-in SSID. Rebuild with `-D WIFI_SSID=...` / `-D WIFI_PWD=...` (see repo docs). Always use `--chip esp32c6 --flash-mode dio --flash-freq 80m --flash-size 4MB`.
