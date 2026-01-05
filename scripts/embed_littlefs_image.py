import os
from SCons.Script import DefaultEnvironment

env = DefaultEnvironment()

# Only run for esp32d_debug environment
if env['PIOENV'] == 'esp32d_debug':
    build_dir = env.subst("$BUILD_DIR")
    image_path = os.path.join(build_dir, "spiffs.bin")
    out_path = os.path.join(env.subst("$PROJECTSRC_DIR"), "littlefs_image.c")
    script_path = os.path.join(env.subst("$PROJECT_DIR"), "scripts", "bin2c.py")
    symbol = "littlefs_image_bin"
    if os.path.exists(image_path):
        print(f"[extra_script] Embedding LittleFS image: {image_path} -> {out_path}")
        ret = os.system(f"python3 {script_path} {image_path} {out_path} {symbol}")
        if ret != 0:
            print("[extra_script] bin2c.py failed!")
    else:
        print(f"[extra_script] LittleFS image not found: {image_path}")
