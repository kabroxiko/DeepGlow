/**
 * SPI.h shim:
 *  - ESP-IDF builds: minimal stub (no real SPI driver needed).
 *  - Arduino builds: transparently forward to the framework's real SPI.h.
 */
#pragma once

#if defined(ESP_PLATFORM) && !defined(ARDUINO)

#include <stdint.h>
#include <stddef.h>

#define MSBFIRST 1
#define LSBFIRST 0
#define SPI_MODE0 0
#define SPI_MODE1 1
#define SPI_MODE2 2
#define SPI_MODE3 3

class SPISettings {
public:
    SPISettings(uint32_t, uint8_t, uint8_t) {}
    SPISettings() {}
};

class SPIClass {
public:
    SPIClass() {}
    SPIClass(int) {}   // HSPI/FSPI constants
    SPIClass(uint8_t) {}
    void begin(int = -1, int = -1, int = -1, int = -1) {}
    void end() {}
    void beginTransaction(const SPISettings &) {}
    void endTransaction() {}
    uint8_t transfer(uint8_t) { return 0; }
    void writeBytes(const uint8_t *, size_t) {}
};

extern SPIClass SPI;

#else
// Arduino: use the real framework SPI.h (next one found in include path)
#include_next <SPI.h>
#endif
