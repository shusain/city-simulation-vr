@echo off
setlocal
if not defined LOCALAPPDATA exit /b 1
if not defined CITYSIM_TEST_RUNTIME_DIR set "CITYSIM_RUNTIME_DIR=%SystemRoot%\System32"
if defined CITYSIM_TEST_RUNTIME_DIR set "CITYSIM_RUNTIME_DIR=%CITYSIM_TEST_RUNTIME_DIR%"
if not exist "%CITYSIM_RUNTIME_DIR%\vcruntime140.dll" goto missing_runtime
if not exist "%CITYSIM_RUNTIME_DIR%\vcruntime140_1.dll" goto missing_runtime
if not exist "%CITYSIM_RUNTIME_DIR%\msvcp140.dll" goto missing_runtime
if not defined CITYSIM_PLAYTEST_PROFILE_ROOT set "CITYSIM_PLAYTEST_PROFILE_ROOT=%LOCALAPPDATA%\CitySimVRPlaytest"
set "CITYSIM_PROFILE=%CITYSIM_PLAYTEST_PROFILE_ROOT%\Desktop"
if not exist "%CITYSIM_PROFILE%\Logs" mkdir "%CITYSIM_PROFILE%\Logs"
start "" /D "%~dp0Windows\CitySimVR\Binaries\Win64" "%~dp0Windows\CitySimVR\Binaries\Win64\CitySimVR.exe" -CityDesktop -nohmd -d3d11 -UserDir="%CITYSIM_PROFILE%" -abslog="%CITYSIM_PROFILE%\Logs\Latest.log" %*
exit /b 0
:missing_runtime
echo The Microsoft Visual C++ x64 runtime is required before starting City Sim VR.
echo Run "%~dp0Windows\Engine\Extras\Redist\en-us\vc_redist.x64.exe", finish installation, then launch Play-Desktop.cmd again.
echo Installing that runtime may ask for administrator permission. The game is not started or elevated here.
if not defined CITYSIM_TEST_RUNTIME_DIR pause
exit /b 2
