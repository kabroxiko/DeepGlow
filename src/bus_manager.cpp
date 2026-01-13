#include "bus_manager.h"
#include <NeoPixelBus.h>

// Example implementation for NeoPixelBus wrapper
void BusNeoPixel::show() {
    if (!_strip) {
        return;
    }
    switch (_type) {
        case BusNeoPixelType::SK6812: {
            auto* s = static_cast<NeoPixelBus<NeoRgbwFeature, NeoEsp32Rmt0Sk6812Method>*>(_strip);
            s->Show();
            break;
        }
        case BusNeoPixelType::WS2812B_RGB: {
            auto* s = static_cast<NeoPixelBus<NeoRgbFeature, NeoEsp32Rmt0Ws2812xMethod>*>(_strip);
            s->Show();
            break;
        }
        case BusNeoPixelType::WS2812B_GRB: {
            auto* s = static_cast<NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod>*>(_strip);
            s->Show();
            break;
        }
    }
}

// Update pixel count for all buses (returns total)
uint16_t BusManager::updatePixelCount() {
    uint16_t total = 0;
    for (const auto& bus : buses) {
        total += bus->getLength();
    }
    pixelCount = total;
    return pixelCount;
}

void BusNeoPixel::setPixelColor(uint16_t pix, uint32_t color) {
    if (!_strip) {
        return;
    }
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    switch (_type) {
        case BusNeoPixelType::SK6812: {
            auto* s = static_cast<NeoPixelBus<NeoRgbwFeature, NeoEsp32Rmt0Sk6812Method>*>(_strip);
            s->SetPixelColor(pix, RgbwColor(r, g, b, 0));
            break;
        }
        case BusNeoPixelType::WS2812B_RGB: {
            auto* s = static_cast<NeoPixelBus<NeoRgbFeature, NeoEsp32Rmt0Ws2812xMethod>*>(_strip);
            s->SetPixelColor(pix, RgbColor(r, g, b));
            break;
        }
        case BusNeoPixelType::WS2812B_GRB: {
            auto* s = static_cast<NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod>*>(_strip);
            s->SetPixelColor(pix, RgbColor(r, g, b));
            break;
        }
    }
}

// You can add more bus types (PWM, Network, etc.) as needed

// Example usage in main.cpp:
// BusManager busManager;
// busManager.addBus(std::make_unique<BusNeoPixel>(strip, pixelCount));
// busManager.setPixelColor(0, 0xFF0000); // Set first pixel to red
// busManager.show();
