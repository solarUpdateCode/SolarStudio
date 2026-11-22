#!/usr/bin/env bash
set -euo pipefail

VERSION="2.1.0"
BUILD_DIR="build"
DIST_DIR="dist"
STAGE_DIR="dist/pkg_root"

mkdir -p "${DIST_DIR}"
rm -rf "${STAGE_DIR}"
mkdir -p "${STAGE_DIR}/Applications"
mkdir -p "${STAGE_DIR}/Library/Audio/Plug-Ins/VST3"
mkdir -p "${STAGE_DIR}/Library/Audio/Plug-Ins/Components"

# Copy Standalone App (already contains SolarStudio.icns)
cp -R "${BUILD_DIR}/SolarStudio_artefacts/Release/SolarStudio.app" "${STAGE_DIR}/Applications/"
cp -f "Assets/SolarStudio.icns" "${STAGE_DIR}/Applications/SolarStudio.app/Contents/Resources/SolarStudio.icns"

# Copy Companion SolarStudio Bridge Plugins (VST3 & AU)
if [ -d "${BUILD_DIR}/SolarStudioBridge_artefacts/Release/VST3/SolarStudio Bridge.vst3" ]; then
  cp -R "${BUILD_DIR}/SolarStudioBridge_artefacts/Release/VST3/SolarStudio Bridge.vst3" "${STAGE_DIR}/Library/Audio/Plug-Ins/VST3/"
fi
if [ -d "${BUILD_DIR}/SolarStudioBridge_artefacts/Release/AU/SolarStudio Bridge.component" ]; then
  cp -R "${BUILD_DIR}/SolarStudioBridge_artefacts/Release/AU/SolarStudio Bridge.component" "${STAGE_DIR}/Library/Audio/Plug-Ins/Components/"
fi

# Build macOS .pkg installer
pkgbuild \
  --root "${STAGE_DIR}" \
  --identifier "com.solaraudio.solarstudio.pkg" \
  --version "${VERSION}" \
  --install-location "/" \
  "${DIST_DIR}/SolarStudio-${VERSION}-macOS-Installer.pkg"

# Build macOS .dmg with Applications symlink
DMG_STAGE="dist/dmg_root"
rm -rf "${DMG_STAGE}"
mkdir -p "${DMG_STAGE}"
cp -R "${STAGE_DIR}/Applications/SolarStudio.app" "${DMG_STAGE}/"
ln -s /Applications "${DMG_STAGE}/Applications"

hdiutil create \
  -volname "SolarStudio ${VERSION}" \
  -srcfolder "${DMG_STAGE}" \
  -ov -format UDZO \
  "${DIST_DIR}/SolarStudio-${VERSION}-macOS-Universal.dmg"

echo "Built macOS installers in ${DIST_DIR}/"
