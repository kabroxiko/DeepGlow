#!/bin/zsh
set -e

if [ -z "$1" ]; then
	echo "Usage: $0 <platformio_env>"
	echo "Example: $0 Athom_4Pin_Controller"
	exit 1
fi

REPO_URL="https://github.com/wled/WLED.git"
REPO_DIR="WLED"

############################################################
# 1. Ensure WLED source is present and clean
#    - If WLED directory does not exist, clone from GitHub
#    - If WLED exists and is a git repo, reset to latest main
#    - If WLED exists but is not a git repo, delete and re-clone
############################################################
if [ -d "$REPO_DIR" ]; then
	if [ -d "$REPO_DIR/.git" ]; then
		echo "Resetting existing WLED repository..."
		cd "$REPO_DIR"
		git fetch origin
		git reset --hard origin/main
		cd ..
	else
		echo "Directory $REPO_DIR exists but is not a git repo. Removing..."
		rm -rf "$REPO_DIR"
		echo "Cloning WLED repository..."
		git clone "$REPO_URL" "$REPO_DIR"
	fi
else
	echo "Cloning WLED repository..."
	git clone "$REPO_URL" "$REPO_DIR"
fi

#
# 2. Apply all patch files in overlay/patches to WLED source
#    - Each .patch file in ./patches is applied to the WLED source
#    - Already-applied/reversed patches are skipped without error
if [ -d ./patches ]; then
	for patch in ./patches/*.patch; do
		if [ -f "$patch" ]; then
			echo "Applying patch: $patch"
			patch --batch --forward -d ./WLED -p1 < "$patch"
		fi
	done
fi

#
# 3. Copy all overlays
#    - All files and folders in ./overlay/ are copied into ./WLED/
#    - Overwrites any existing files in the WLED source
cp -vrf ./overlay/* ./WLED/

#
# 4. Build WLED firmware
#    - Change to WLED directory and run PlatformIO build
cd WLED
platformio run -e "$1"
