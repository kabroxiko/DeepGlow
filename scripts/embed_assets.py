#!/usr/bin/env python3


# PlatformIO pre-build script: embed assets as .inc files
# Import("env")

import os
import subprocess
import sys
import tempfile
import re
import configparser
import argparse
import logging
from SCons.Script import DefaultEnvironment

env = DefaultEnvironment()

# Setup logging
logging.basicConfig(
	level=logging.INFO,
	format='[%(levelname)s] %(message)s'
)


# Use project root as base (PlatformIO sets cwd to project root)
ASSET_DIR_DIST = os.path.join(os.getcwd(), 'dist')
ASSET_DIR_SRC = os.path.join(os.getcwd(), 'defaults')
OUT_DIR = os.path.join(os.getcwd(), 'src/inc')

# Output file constants
INDEX_HTML_INC = 'index_html.inc'
INDEX_JS_INC = 'index_js.inc'
STYLE_CSS_INC = 'style_css.inc'
CONFIG_JSON_INC = 'config_default.inc'
PRESETS_JSON_INC = 'presets_json.inc'
TIMEZONES_JSON_INC = 'timezones_json.inc'
APP_JS_INC = 'app_js.inc'

ASSETS = [
	(ASSET_DIR_DIST, 'index.html', INDEX_HTML_INC),
	(ASSET_DIR_DIST, 'index.js', INDEX_JS_INC),
	(ASSET_DIR_DIST, 'style.css', STYLE_CSS_INC),
	(ASSET_DIR_SRC, 'config.json', CONFIG_JSON_INC),
	(ASSET_DIR_SRC, 'presets.json', PRESETS_JSON_INC),
	(ASSET_DIR_SRC, 'timezones.json', TIMEZONES_JSON_INC),
	('DYNAMIC', APP_JS_INC),
]


# Ensure output directory exists
os.makedirs(OUT_DIR, exist_ok=True)


# Only delete and regenerate .inc files for assets that have changed, unless force is True
def asset_needs_update(src_path, inc_path, force=False):
	if force:
		return True
	if not os.path.exists(inc_path):
		return True
	src_mtime = os.path.getmtime(src_path)
	inc_mtime = os.path.getmtime(inc_path)
	return src_mtime > inc_mtime


def minify_asset(infile):
	# No extra minification; use Vite output as-is
	with open(infile, 'r', encoding='utf-8') as f:
		return f.read()

def to_inc(infile, outfile):
	try:
		infile_path = infile  # infile is now absolute path
		outfile_path = os.path.join(OUT_DIR, outfile)
		var_base = os.path.splitext(outfile)[0]  # e.g., index_html
		var_name = f"web_{var_base}"
		logging.info(f'Embedding: {infile_path} -> {outfile_path} (var: {var_name}')
		if not os.path.exists(infile_path):
			logging.error(f'Source file not found: {infile_path}')
			return False
		ext = os.path.splitext(infile_path)[1]
		with tempfile.NamedTemporaryFile('w+', delete=False, encoding='utf-8', suffix=ext) as tmp:
			tmp.write(infile_path)
			tmp.flush()
			tmp_path = tmp.name
		with tempfile.NamedTemporaryFile('w+', delete=False, encoding='utf-8', suffix='.inc') as xxd_tmp:
			subprocess.run(['xxd', '-i', '-n', var_name, tmp_path], stdout=xxd_tmp, check=True)
			xxd_tmp.flush()
			xxd_tmp_path = xxd_tmp.name
		with open(xxd_tmp_path, 'r', encoding='utf-8') as xxd_file:
			xxd_content = xxd_file.read()
		array_decl_re = re.compile(r'^unsigned char\s+(\w+)\[\]\s*=\s*\{', re.MULTILINE)
		match = array_decl_re.search(xxd_content)
		if not match:
			logging.error(f'Could not find array declaration in {outfile_path}')
			return False
		var_name = match.group(1)
		# Extract array and length variable
		array_decl_re = re.compile(r'(unsigned char\s+\w+\[\]\s*=\s*\{.*?\};)', re.DOTALL)
		len_decl_re = re.compile(r'(unsigned int\s+\w+_len\s*=\s*\d+;)', re.DOTALL)
		array_match = array_decl_re.search(xxd_content)
		len_match = len_decl_re.search(xxd_content)
		if not array_match or not len_match:
			logging.error(f'Could not extract array or length in {outfile_path}')
			return False
		array_decl = array_match.group(1)
		len_decl = len_match.group(1)
		# If the file is a .json, do not use PROGMEM even for ESP8266/AVR
		is_json = ext == '.json'
		if is_json:
			branch = f"const unsigned char {var_name}[] = {array_decl[array_decl.find('{'):array_decl.find('};')+2]}\n{len_decl.replace('static ', '')}\n"
			with open(outfile_path, 'w', encoding='utf-8') as out:
				out.write(branch)
		else:
			branch_esp = f"#if defined(ESP8266) || defined(ARDUINO_ARCH_AVR)\nconst unsigned char {var_name}[] PROGMEM = {array_decl[array_decl.find('{'):array_decl.find('};')+2]}\n{len_decl.replace('static ', '')}\n"
			branch_else = f"#else\nconst unsigned char {var_name}[] = {array_decl[array_decl.find('{'):array_decl.find('};')+2]}\n{len_decl.replace('static ', '')}\n#endif\n"
			with open(outfile_path, 'w', encoding='utf-8') as out:
				out.write(branch_esp)
				out.write(branch_else)
		os.remove(xxd_tmp_path)
		os.remove(tmp_path)
		logging.info(f'Success: {outfile_path}')
		return True
	except Exception as e:
		logging.error(f'Failed to embed {infile}: {e}')
		return False


def run_npm_build():
	try:
		logging.info('Running npm run build to generate dist assets...')
		subprocess.run(['npm', 'i'], check=True)
		subprocess.run(['npm', 'run', 'build'], check=True)
		logging.info('npm build completed.')
	except Exception as e:
		logging.error(f'npm build failed: {e}')
		sys.exit(1)


def get_force_flag():
	parser = argparse.ArgumentParser()
	parser.add_argument('--force', action='store_true', help='Force regeneration of all .inc files')
	args, _ = parser.parse_known_args()
	return args.force or os.environ.get('EMBED_ASSETS_FORCE', '0') in ('1', 'true', 'yes', 'on')


def find_dynamic_assets():
	dist_assets = ASSET_DIR_DIST
	js_files = [f for f in os.listdir(dist_assets) if re.match(r'index-.*\.js$', f)]
	css_files = [f for f in os.listdir(dist_assets) if re.match(r'index-.*\.css$', f)]
	app_js_files = [f for f in os.listdir(dist_assets) if re.match(r'app-.*\.js$', f)]
	return js_files[0] if js_files else None, css_files[0] if css_files else None, app_js_files[0] if app_js_files else None


def get_asset_paths(asset, index_js, style_css, app_js):
	if asset[0] == 'DYNAMIC':
		if asset[1] == INDEX_JS_INC and index_js:
			return os.path.join(ASSET_DIR_DIST, index_js), os.path.join(OUT_DIR, INDEX_JS_INC)
		elif asset[1] == STYLE_CSS_INC and style_css:
			return os.path.join(ASSET_DIR_DIST, style_css), os.path.join(OUT_DIR, STYLE_CSS_INC)
		elif asset[1] == APP_JS_INC and app_js:
			return os.path.join(ASSET_DIR_DIST, app_js), os.path.join(OUT_DIR, APP_JS_INC)
		return None, None
	else:
		asset_dir, src, dst = asset
		return os.path.join(asset_dir, src), os.path.join(OUT_DIR, dst)


def process_asset(src_path, inc_path, force):
	if asset_needs_update(src_path, inc_path, force=force):
		if os.path.exists(inc_path):
			try:
				os.remove(inc_path)
			except Exception as e:
				logging.warning(f'Could not delete {inc_path}: {e}')
		return to_inc(src_path, os.path.basename(inc_path))
	else:
		logging.info(f'Skipping unchanged asset: {os.path.basename(src_path)}')
		return True


def main():
	if env.IsCleanTarget():
		return

	run_npm_build()

	force = get_force_flag()
	index_js, style_css, app_js = find_dynamic_assets()

	all_ok = True
	for asset in ASSETS:
		src_path, inc_path = get_asset_paths(asset, index_js, style_css, app_js)
		if src_path is None:
			continue
		ok = process_asset(src_path, inc_path, force)
		all_ok = all_ok and ok

	if all_ok:
		logging.info('Web assets embedded as .inc files.')
	else:
		logging.error('Some assets failed to embed. See errors above.')
		sys.exit(1)

main()
