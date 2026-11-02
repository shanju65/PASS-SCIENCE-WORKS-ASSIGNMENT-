@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo   BUILDING UNIFIED ENGINEERING WORKBENCH (MSVC x64)
echo   PASS SCIENCE WORKS R^&D INTERNSHIP 2026-2027
echo ========================================================

REM 1. Initialize MSVC 64-bit environment
where cl >nul 2>nul
if errorlevel 1 (
    echo [ERROR] MSVC C++ build environment is not initialized. Run from a Visual Studio Developer Command Prompt.
    exit /b 1
)

REM 2. Locate CMake
set CMAKE_BIN=cmake

REM 3. Configure CMake
echo [1/4] Configuring project via CMake...
if not exist build mkdir build
%CMAKE_BIN% -B build -S . -G "Visual Studio 16 2019" -A x64
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake configuration failed!
    exit /b %ERRORLEVEL%
)

REM 4. Build Release target
echo [2/4] Compiling targets (Release x64)...
%CMAKE_BIN% --build build --config Release
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Build compilation failed!
    exit /b %ERRORLEVEL%
)

REM 5. Copy Launch.exe to root if not already there
if exist build\Release\Launch.exe (
    copy /Y build\Release\Launch.exe .\Launch.exe >nul
    echo [3/4] Successfully placed Launch.exe in root directory.
)

REM 6. Run verification test runner
echo [4/4] Executing test suite...
if exist build\Release\workbench_tests.exe (
    build\Release\workbench_tests.exe
) else (
    echo [ERROR] workbench_tests.exe was not found.
    exit /b 1
)

echo ========================================================
echo   BUILD AND TESTS COMPLETED SUCCESSFULLY!
echo ========================================================
