#!/usr/bin/env bash
# Builds the iPad targets (AUv3 extension + standalone app) with Xcode.
#
# Requirements: macOS with Xcode and the command line tools installed.
# Usage: ./scripts/build-ios.sh [team-id]
#
# The resulting app bundle contains the AUv3 extension:
#   build-ios/MKAmbient_artefacts/Release/MK Ambient.app/PlugIns/MK Ambient.appex
# Launch the host app at least once on the device before the plug-in will show
# up in GarageBand / AUM / Cubasis.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${ROOT}/build-ios"
TEAM_ID="${1:-}"

cmake -S "${ROOT}" -B "${BUILD_DIR}" -G Xcode \
      -DCMAKE_SYSTEM_NAME=iOS \
      -DCMAKE_OSX_ARCHITECTURES=arm64 \
      -DCMAKE_XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET=17.0 \
      -DMK_BUILD_PLUGIN=ON \
      -DMK_BUILD_TESTS=OFF \
      -DMK_BUILD_RENDERER=OFF \
      -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGN_STYLE=Automatic \
      ${TEAM_ID:+-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=${TEAM_ID}}

cmake --build "${BUILD_DIR}" --config Release

echo
echo "Built:"
find "${BUILD_DIR}" -maxdepth 3 -name "*.app" -o -maxdepth 3 -name "*.appex" | sed 's/^/  /'
echo
echo "Open ${BUILD_DIR}/MKAmbient.xcodeproj to set signing/team and deploy to the iPad."
