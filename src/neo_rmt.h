#pragma once

#if defined(ESP_PLATFORM) && !defined(ARDUINO)

#include <stdint.h>
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"

/**
 * NeoRmtStrip — thin ESP-IDF 5.x RMT driver for WS2812B / SK6812 LEDs.
 *
 * Pixel bytes are stored in wire order:
 *  SK6812 (rgbw=true) : G-R-B-W  (4 bytes / pixel)
 *  WS2812B GRB        : G-R-B    (3 bytes / pixel)
 *  WS2812B RGB        : R-G-B    (3 bytes / pixel)
 *
 * Callers are responsible for placing bytes in the correct wire order
 * before calling SetPixelBytes().
 */
class NeoRmtStrip {
public:
    NeoRmtStrip(uint16_t count, uint8_t pin, bool rgbw);
    ~NeoRmtStrip();

    bool Begin();
    void Show();

    /** Write bytesPerPixel bytes starting at index. */
    void SetPixelBytes(uint16_t index, const uint8_t *bytes);
    /** Read bytesPerPixel bytes at index. */
    void GetPixelBytes(uint16_t index, uint8_t *bytes) const;

    uint16_t PixelCount()    const { return _count; }
    uint8_t  BytesPerPixel() const { return _bytesPerPixel; }

private:
    uint16_t  _count;
    uint8_t   _pin;
    bool      _rgbw;
    uint8_t   _bytesPerPixel;
    uint8_t  *_pixels;

    rmt_channel_handle_t _chan;
    rmt_encoder_handle_t _bytesEncoder;
};

#else

#include <Arduino.h>
// Arduino stub for NeoRmtStrip (not used)
class NeoRmtStrip {
public:
    NeoRmtStrip(uint16_t, uint8_t, bool) {}
    ~NeoRmtStrip() {}
    bool Begin() { return false; }
    void Show() {}
    void SetPixelBytes(uint16_t, const uint8_t *) {}
    void GetPixelBytes(uint16_t, uint8_t *) const {}
    uint16_t PixelCount() const { return 0; }
    uint8_t BytesPerPixel() const { return 0; }
};

#endif // ESP_PLATFORM
