@echo off
rem One-click self-check for the GameXXK harness scripts.
rem Runs the three headless Python tests that verify the process-tooling changes.
cd /d "%~dp0"
set "PYTHONPATH=%~dp0;%PYTHONPATH%"

echo === 1/3 parse_automation_index ===
python scripts\test_parse_automation_index.py
if errorlevel 1 goto :fail

echo.
echo === 2/3 harness_state_validator ===
python scripts\test_harness_state_validator.py
if errorlevel 1 goto :fail

echo.
echo === 3/3 ue_tdd_pipeline ===
python scripts\test_ue_tdd_pipeline.py
if errorlevel 1 goto :fail

echo.
echo ALL PASS
pause
exit /b 0

:fail
echo.
echo SOME TEST FAILED - see output above
pause
exit /b 1
