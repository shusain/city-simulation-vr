@echo off
setlocal
if not defined LOCALAPPDATA exit /b 1
tasklist /FI "IMAGENAME eq vrserver.exe" 2>nul | find /I "vrserver.exe" >nul
if errorlevel 1 (
  echo Start SteamVR and connect your headset before launching PCVR.
  pause
  exit /b 1
)
if not defined CITYSIM_PLAYTEST_PROFILE_ROOT set "CITYSIM_PLAYTEST_PROFILE_ROOT=%LOCALAPPDATA%\CitySimVRPlaytest"
set "CITYSIM_PROFILE=%CITYSIM_PLAYTEST_PROFILE_ROOT%\PCVR"
if not exist "%CITYSIM_PROFILE%\Logs" mkdir "%CITYSIM_PROFILE%\Logs"
start "" /D "%~dp0Windows\CitySimVR\Binaries\Win64" "%~dp0Windows\CitySimVR\Binaries\Win64\CitySimVR.exe" -vr -nohighdpi -d3d11 -UserDir="%CITYSIM_PROFILE%" -abslog="%CITYSIM_PROFILE%\Logs\Latest.log" %*
