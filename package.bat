@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo   PACKAGING UNIFIED ENGINEERING WORKBENCH SUBMISSION ZIP
echo   PASS SCIENCE WORKS R^&D INTERNSHIP 2026-2027
echo ========================================================

set CANDIDATE_ID=%1
if "%CANDIDATE_ID%"=="" (
    set CANDIDATE_ID=CandidateID
    echo [INFO] No Candidate ID provided as argument. Using default: CandidateID
    echo [INFO] To specify: package.bat YOUR_ID
)

set ZIP_NAME=%CANDIDATE_ID%_Workbench.zip
echo [INFO] Target archive name: %ZIP_NAME%

REM Package a clean submission folder with no build caches, temporary files, or duplicate archives.
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "$root = Get-Location;" ^
    "$folder = Join-Path $root '.candidate-package\CandidateID_Workbench';" ^
    "Remove-Item -Recurse -Force -ErrorAction SilentlyContinue '.candidate-package';" ^
    "New-Item -ItemType Directory -Force $folder | Out-Null;" ^
    "$copyItems = @('Launch.exe','README.md','CMakeLists.txt','.gitignore','build.bat','package.bat','DEPENDENCY_VERSIONS.txt','src','data','tests','results','docs','reports');" ^
    "foreach ($item in $copyItems) { if (Test-Path (Join-Path $root $item)) { Copy-Item -LiteralPath (Join-Path $root $item) -Destination $folder -Recurse -Force } }" ^
    "Copy-Item -LiteralPath (Join-Path $root 'src\frontend') -Destination (Join-Path $folder 'frontend') -Recurse -Force;" ^
    "Remove-Item -Force -ErrorAction SilentlyContinue '%ZIP_NAME%';" ^
    "$staging = Join-Path $root '.candidate-package';" ^
    "Compress-Archive -Path (Join-Path $staging 'CandidateID_Workbench') -DestinationPath (Join-Path $root '%ZIP_NAME%') -CompressionLevel Optimal;" ^
    "Remove-Item -Recurse -Force $staging;"

if exist %ZIP_NAME% (
    echo ========================================================
    echo   PACKAGE CREATED SUCCESSFULLY: %ZIP_NAME%
    echo ========================================================
) else (
    echo [ERROR] Packaging failed!
    exit /b 1
)
