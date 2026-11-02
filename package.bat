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

REM Generate repository evidence before packaging.
git bundle create repository.bundle --all
if errorlevel 1 (
    echo [ERROR] Git bundle creation failed!
    exit /b 1
)

git --no-pager log --date=iso --pretty=format:"commit %%H%%nDate: %%ad%%nAuthor: %%an%%nSubject: %%s%%n%%n" --stat > docs\commit_log.txt

REM Use PowerShell only to create the clean ZIP.
powershell -NoProfile -Command ^
    "Remove-Item -Force -ErrorAction SilentlyContinue '%ZIP_NAME%';" ^
    "$items = @('Launch.exe', 'CMakeLists.txt', 'README.md', '.gitignore', 'build.bat', 'DEPENDENCY_VERSIONS.txt', 'src', 'frontend', 'data', 'tests', 'results', 'docs', 'reports', '.vscode', 'repository.bundle', 'DEMO VIDEO PASS.mp4');" ^
    "Compress-Archive -Path $items -DestinationPath '%ZIP_NAME%' -CompressionLevel Optimal;"

if exist %ZIP_NAME% (
    echo ========================================================
    echo   PACKAGE CREATED SUCCESSFULLY: %ZIP_NAME%
    echo ========================================================
) else (
    echo [ERROR] Packaging failed!
    exit /b 1
)
