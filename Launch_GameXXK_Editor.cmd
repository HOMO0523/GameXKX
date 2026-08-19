@echo off
setlocal

for %%I in ("%~dp0.") do set "GAMEXXK_ROOT=%%~fI"
set "GAMEXXK_UE_EDITOR=D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"

if not exist "%GAMEXXK_ROOT%\GameXXK.uproject" (
  echo GameXXK.uproject was not found next to this launcher.
  pause
  exit /b 1
)

if not exist "%GAMEXXK_UE_EDITOR%" (
  echo Unreal Engine 5.8 was not found at D:\UE_5.8.
  pause
  exit /b 1
)

set "GAMEXXK_PROFILE=%GAMEXXK_ROOT%\Saved\InteractiveEditorProfile"
set "GAMEXXK_USER_DIR=%GAMEXXK_ROOT%\Saved\InteractiveEditorUser"
set "GAMEXXK_SHADER_DIR=%GAMEXXK_ROOT%\Saved\InteractiveShaderWorkingDir"
set "GAMEXXK_DDC_DIR=%GAMEXXK_ROOT%\Saved\InteractiveDDC"

set "USERPROFILE=%GAMEXXK_PROFILE%"
set "APPDATA=%GAMEXXK_PROFILE%\AppData\Roaming"
set "LOCALAPPDATA=%GAMEXXK_PROFILE%\AppData\Local"
set "TEMP=%GAMEXXK_PROFILE%\Temp"
set "TMP=%GAMEXXK_PROFILE%\Temp"
set "DOTNET_CLI_HOME=%GAMEXXK_PROFILE%\DotnetHome"
set "NUGET_PACKAGES=%GAMEXXK_PROFILE%\NuGet"
set "UE-LocalDataCachePath=%GAMEXXK_DDC_DIR%"
set "UE_SKIP_UBT_SDK_SETUP=1"

for %%D in (
  "%APPDATA%"
  "%LOCALAPPDATA%"
  "%TEMP%"
  "%DOTNET_CLI_HOME%"
  "%NUGET_PACKAGES%"
  "%GAMEXXK_USER_DIR%"
  "%GAMEXXK_SHADER_DIR%"
  "%GAMEXXK_DDC_DIR%"
) do if not exist "%%~D" mkdir "%%~D"

start "" "%GAMEXXK_UE_EDITOR%" "%GAMEXXK_ROOT%\GameXXK.uproject" -NoZenAutoLaunch -DDC-ForceMemoryCache -UserDir="%GAMEXXK_USER_DIR%" -ShaderWorkingDir="%GAMEXXK_SHADER_DIR%" -ModelContextProtocolStartServer -ModelContextProtocolPort=18765
exit /b 0
