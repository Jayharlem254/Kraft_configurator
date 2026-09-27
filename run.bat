@echo off
REM Kraft Configurator — Launch the high-performance C++ Engine & Web UI
REM Modeled after Quni4m / HQC launch architecture.
cd /d "%~dp0"

set "EXE=backend-cpp\bin\kraft_backend.exe"
if not exist "%EXE%" set "EXE=..\backend-cpp\bin\kraft_backend.exe"

if not exist "%EXE%" (
  echo Could not find kraft_backend.exe. Building C++ backend...
  if exist "backend-cpp\build.bat" (
    cmd /c "cd /d backend-cpp && build.bat"
  )
)

if not exist "%EXE%" (
  echo Fatal: Failed to locate or compile kraft_backend.exe.
  pause
  exit /b 1
)

set "UI_DIST=frontend\dist\index.html"
if not exist "%UI_DIST%" (
  echo Frontend UI not built yet. Building now with Vite...
  if exist "frontend" (
    cmd /c "cd /d frontend && npm install && npm run build"
  )
)

echo Starting Kraft Configurator C++ Engine on port 8000...
start "" http://127.0.0.1:8000
"%EXE%" --port 8000
