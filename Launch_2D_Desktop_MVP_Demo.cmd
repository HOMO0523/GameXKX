@echo off
setlocal
set "ROOT=%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%scripts\launch_desktop_mvp_demo.ps1" %*
exit /b %ERRORLEVEL%
