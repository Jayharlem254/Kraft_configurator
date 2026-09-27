@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo        Building Kraft Configurator C++ Backend
echo ========================================================

set "SCRIPT_DIR=%~dp0"
cd /d "%SCRIPT_DIR%"
if not exist bin mkdir bin

:: 1. Check for Clang++
set "CLANG_BIN="
where clang++.exe >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    set "CLANG_BIN=clang++.exe"
) else if exist "C:\Program Files\Windhawk\Compiler\bin\clang++.exe" (
    set "CLANG_BIN=C:\Program Files\Windhawk\Compiler\bin\clang++.exe"
)

if defined CLANG_BIN (
    echo [Compiler] Found Clang: "!CLANG_BIN!"
    
    :: Compile SQLite C amalgamation if not already compiled
    if not exist bin\sqlite3.o (
        echo [Build] Compiling SQLite C engine...
        "!CLANG_BIN!" --target=x86_64-w64-mingw32 -O2 -x c -c src\sqlite3.c -o bin\sqlite3.o -I include
        if !ERRORLEVEL! NEQ 0 (
            echo Error compiling SQLite.
            exit /b !ERRORLEVEL!
        )
    )

    echo [Build] Compiling and linking kraft_backend.exe...
    "!CLANG_BIN!" --target=x86_64-w64-mingw32 -std=c++17 -O2 -static -static-libgcc -static-libstdc++ ^
        -I include src\server.cpp src\db.cpp bin\sqlite3.o ^
        -o bin\kraft_backend.exe -lws2_32

    if !ERRORLEVEL! EQU 0 (
        echo.
        echo ========================================================
        echo Build SUCCESS: bin\kraft_backend.exe
        echo ========================================================
        echo Run server with:
        echo     bin\kraft_backend.exe --port 8000
        echo.
        exit /b 0
    ) else (
        echo.
        echo Build failed with code !ERRORLEVEL!.
        exit /b !ERRORLEVEL!
    )
)

:: 2. Fallback to MSVC cl.exe
echo Clang not found, falling back to MSVC...
where cl.exe >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if exist "!VSWHERE!" (
        for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -all -products * -find **\vcvarsall.bat`) do (
            if not defined VCVARS set "VCVARS=%%i"
        )
    )
    if defined VCVARS (
        echo Found: "!VCVARS!"
        call "!VCVARS!" x64
    )
)

echo [Build] Compiling Kraft C++ backend with MSVC...
cl.exe /nologo /O2 /std:c++17 /EHsc /utf-8 /I include ^
    src\server.cpp src\db.cpp src\sqlite3.c ^
    /Fe:bin\kraft_backend.exe ^
    /link ws2_32.lib

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================================
    echo Build SUCCESS: bin\kraft_backend.exe
    echo ========================================================
    echo Run server with:
    echo     bin\kraft_backend.exe --port 8000
    echo.
) else (
    echo Build failed.
    exit /b %ERRORLEVEL%
)
