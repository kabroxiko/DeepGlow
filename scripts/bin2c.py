#!/usr/bin/env python3
# Convert LittleFS image to C array for embedding in firmware
import sys
import os

def bin2c(input_path, output_path, symbol_name):
    with open(input_path, 'rb') as f:
        data = f.read()
    with open(output_path, 'w') as out:
        out.write(f"const unsigned char {symbol_name}[] = { {len(data)} bytes }\n")
        out.write(f"__attribute__((aligned(4), section(\".rodata\"))) = {{\n")
        for i, b in enumerate(data):
            if i % 16 == 0:
                out.write("    ")
            out.write(f"0x{b:02x}, ")
            if (i+1) % 16 == 0:
                out.write("\n")
        out.write("\n};\n")
        out.write(f"const unsigned int {symbol_name}_len = {len(data)};\n")

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("Usage: bin2c.py <input.bin> <output.c> <symbol_name>")
        sys.exit(1)
    bin2c(sys.argv[1], sys.argv[2], sys.argv[3])
