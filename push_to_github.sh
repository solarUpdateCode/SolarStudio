#!/usr/bin/env bash
set -euo pipefail

REPO_URL="https://github.com/solarUpdateCode/SolarStudio.git"

git init -b main
git add .
git commit -m "feat: SolarStudio v2.1.0 AI DAW, SolarStudio Bridge VST3/AU, Full AI Backend & Installers"
git remote remove origin 2>/dev/null || true
git remote add origin "${REPO_URL}"
git push -u origin main --force

# Tag v2.1.0 to automatically trigger Windows (.exe) & macOS (.pkg/.dmg) GitHub Actions Release
git tag -f v2.1.0
git push -f origin v2.1.0

echo "Pushed SolarStudio v2.1.0 to ${REPO_URL} and triggered GitHub Actions builds!"
