@echo off
setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
set "REPO_ROOT=%SCRIPT_DIR%\.."
set "LOCAL_EFI=%SCRIPT_DIR%\elfhv.efi"
set "BUILD_DIR=%REPO_ROOT%\build"
set "BUILD_EFI=%BUILD_DIR%\elfhv.efi"

if exist "%LOCAL_EFI%" (
  del /f /q "%LOCAL_EFI%"
)

if not exist "%BUILD_DIR%" (
  mkdir "%BUILD_DIR%"
)

set "RAW_URL=https://raw.githubusercontent.com/ELF-Nigel/ELF-HyperVisor/main/build/elfhv.efi"

where curl >nul 2>nul
if errorlevel 1 (
  echo curl not found. Install curl or use Windows 10/11 built-in curl.
  exit /b 1
)

curl -L -o "%BUILD_EFI%" "%RAW_URL%"
if errorlevel 1 (
  echo download failed.
  exit /b 1
)

echo updated: %BUILD_EFI%
exit /b 0
