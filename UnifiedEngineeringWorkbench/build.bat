@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo   BUILDING UNIFIED ENGINEERING WORKBENCH (MSVC x64)
echo   PASS SCIENCE WORKS R&D INTERNSHIP 2026-2027
echo ========================================================

REM 1. Initialize MSVC 64-bit environment
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
) else (
    echo [WARN] Visual Studio 2019 BuildTools path not found, relying on existing environment...
)

REM 2. Locate CMake
set CMAKE_BIN="C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not exist %CMAKE_BIN% (
    set CMAKE_BIN=cmake
)

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
