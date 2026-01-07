#!/usr/bin/env python3

# PlatformIO pre-build script: embed web assets as .inc files
Import("env")
import os
import subprocess
import sys

# Use project root as base (PlatformIO sets cwd to project root)
ASSET_DIR = os.path.join(os.getcwd(), 'data')
OUT_DIR = os.path.join(os.getcwd(), 'src/web_assets')
# Ensure output directory exists
os.makedirs(OUT_DIR, exist_ok=True)

ASSETS = [
	('index.html', 'index_html.inc'),
	('wifi.html', 'wifi_html.inc'),
	('app.js', 'app_js.inc'),
	('style.css', 'style_css.inc'),
]


def to_inc(infile, outfile):
	infile_path = os.path.join(ASSET_DIR, infile)
	outfile_path = os.path.join(OUT_DIR, outfile)
	var_base = os.path.splitext(outfile)[0]  # e.g., index_html
	var_name = f"web_{var_base}"
	print(f'Embedding: {infile_path} -> {outfile_path} (var: {var_name})')
	if not os.path.exists(infile_path):
		print(f'ERROR: Source file not found: {infile_path}', file=sys.stderr)
		return False
	try:
		with open(outfile_path, 'w') as out:
			subprocess.run(['xxd', '-i', '-n', var_name, infile_path], stdout=out, check=True)
		print(f'Success: {outfile_path}')
		return True
	except Exception as e:
		print(f'ERROR: Failed to embed {infile_path}: {e}', file=sys.stderr)
		return False

all_ok = True
for src, dst in ASSETS:
	ok = to_inc(src, dst)
	all_ok = all_ok and ok
if all_ok:
	print('Web assets embedded as .inc files.')
else:
	print('Some assets failed to embed. See errors above.', file=sys.stderr)
	sys.exit(1)
