@echo off
REM Register the OBS Virtual Camera DirectShow filter so Zoom/other apps see it.
REM Run as Administrator once.

net session >nul 2>&1
if errorlevel 1 (
    echo This script must be run as Administrator.
    pause
    exit /b 1
)

set "OBS_DATA=C:\Program Files\obs-studio\data\obs-plugins\win-dshow"

if not exist "%OBS_DATA%\obs-virtualcam-module64.dll" (
    echo ERROR: OBS not found at expected path.
    echo Expected: %OBS_DATA%\obs-virtualcam-module64.dll
    pause
    exit /b 1
)

echo Registering OBS Virtual Camera 64-bit filter...
regsvr32 /s "%OBS_DATA%\obs-virtualcam-module64.dll"
if errorlevel 1 (
    echo regsvr32 returned error (filter may already be registered, which is OK)
)

reg query "HKLM\SOFTWARE\Classes\CLSID\{A3FCE0F5-3493-419F-958A-ABA1250EC20B}" >nul 2>&1
if errorlevel 1 (
    echo ERROR: Filter registration failed.
    pause
    exit /b 1
)

echo OK: OBS Virtual Camera DirectShow filter is registered.
echo.
echo Now:
echo  1. Запустите RCubiQ Studio — приложение создаст общую память OBSVirtualCamVideo.
echo  2. In Zoom choose "OBS Virtual Camera" as the camera source.
echo     (If Zoom is already open, restart it so it re-enumerates cameras.)
pause
exit /b 0
