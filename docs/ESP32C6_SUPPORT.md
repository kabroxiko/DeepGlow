# ESP32-C6 Support

## Added Environment
- `env:esp32c6` in platformio.ini
  - Board: esp32-c6-devkitc-1
  - Platform: espressif32@6.12.0
  - Framework: arduino

## Source Compatibility
- Conditional compilation updated for ESP32C6 in webserver.cpp
- OTA task creation and Update.h included for ESP32C6

## Build
- Use: `platformio run -e esp32c6`

## Notes
- Make sure your PlatformIO and espressif32 platform are up to date.
- ESP32-C6 support is experimental; test all features thoroughly.

## Next Steps
- Flash firmware to ESP32-C6 board
- Report any issues for further compatibility improvements
