#include "colors.h"

uint32_t parseHexColor(const String& hex) {
    String s = hex;
    if (s.length() == 0) return 0;
    if (s[0] == '#') s = s.substring(1);
    uint32_t val = (uint32_t)strtoul(s.c_str(), nullptr, 16);
    // If 6 hex digits (RRGGBB), shift left 8 bits to add w=0. If 8 digits, use as-is (RRGGBBWW)
    if (s.length() == 6) {
        val = (val << 8); // 0xRRGGBB -> 0xRRGGBB00
    }
    // If 8 digits, 0xRRGGBBWW is already correct
    debugPrint("[parseHexColor] input: "); debugPrint(hex); debugPrint(" parsed: 0x"); debugPrintln(String(val, HEX));
    return val;
}
