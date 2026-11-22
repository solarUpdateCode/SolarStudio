@echo off
setlocal
set REPO_URL=https://github.com/solarUpdateCode/SolarStudio.git

git init -b main
git add .
git commit -m "feat: SolarStudio v2.1.0 AI DAW, SolarStudio Bridge VST3/AU, Full AI Backend & Installers"
git remote remove origin 2>nul
git remote add origin %REPO_URL%
git push -u origin main --force

git tag -f v2.1.0
git push -f origin v2.1.0

echo.
echo Pushed SolarStudio v2.1.0 to %REPO_URL% and triggered GitHub Actions builds!
pause
