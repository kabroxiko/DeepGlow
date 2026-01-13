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

void BusManager::cleanupStrip() {
    if (!buses.empty()) {
        BusNeoPixel* neo = dynamic_cast<BusNeoPixel*>(buses.front().get());
        if (neo) {
            void* strip = neo->_strip;
            BusNeoPixelType prevType = neo->_type;
            String prevOrder = "GRB";
            if (prevType == BusNeoPixelType::SK6812) {
                delete (NeoPixelBus<NeoRgbwFeature, NeoEsp32Rmt0Sk6812Method>*)strip;
            } else {
                if (prevType == BusNeoPixelType::WS2812B_RGB) {
                    delete (NeoPixelBus<NeoRgbFeature, NeoEsp32Rmt0Ws2812xMethod>*)strip;
                } else {
                    delete (NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod>*)strip;
                }
            }
        }
        buses.clear();
    }
}

void BusManager::setupStrip(const String& type, const String& colorOrder, uint8_t pin, uint16_t count) {
    cleanupStrip();
    void* strip = nullptr;
    BusNeoPixelType ledType = type.equalsIgnoreCase("SK6812") ? BusNeoPixelType::SK6812 : (colorOrder.equalsIgnoreCase("RGB") ? BusNeoPixelType::WS2812B_RGB : BusNeoPixelType::WS2812B_GRB);
    if (ledType == BusNeoPixelType::SK6812) {
        strip = new NeoPixelBus<NeoRgbwFeature, NeoEsp32Rmt0Sk6812Method>(count, pin);
        ((NeoPixelBus<NeoRgbwFeature, NeoEsp32Rmt0Sk6812Method>*)strip)->Begin();
        ((NeoPixelBus<NeoRgbwFeature, NeoEsp32Rmt0Sk6812Method>*)strip)->Show();
        addBus(std::unique_ptr<BusNeoPixel>(new BusNeoPixel(strip, count, BusNeoPixelType::SK6812)));
    } else if (ledType == BusNeoPixelType::WS2812B_RGB) {
        strip = new NeoPixelBus<NeoRgbFeature, NeoEsp32Rmt0Ws2812xMethod>(count, pin);
        ((NeoPixelBus<NeoRgbFeature, NeoEsp32Rmt0Ws2812xMethod>*)strip)->Begin();
        ((NeoPixelBus<NeoRgbFeature, NeoEsp32Rmt0Ws2812xMethod>*)strip)->Show();
        addBus(std::unique_ptr<BusNeoPixel>(new BusNeoPixel(strip, count, BusNeoPixelType::WS2812B_RGB)));
    } else {
        strip = new NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod>(count, pin);
        ((NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod>*)strip)->Begin();
        ((NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod>*)strip)->Show();
        addBus(std::unique_ptr<BusNeoPixel>(new BusNeoPixel(strip, count, BusNeoPixelType::WS2812B_GRB)));
    }
}

// You can add more bus types (PWM, Network, etc.) as needed

// Example usage in main.cpp:
// BusManager busManager;
// busManager.addBus(std::make_unique<BusNeoPixel>(strip, pixelCount));
// busManager.setPixelColor(0, 0xFF0000); // Set first pixel to red
// busManager.show();
