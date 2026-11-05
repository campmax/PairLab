@echo off
setlocal
cd /d "%~dp0"
where py >nul 2>nul
if errorlevel 1 goto no_python
py tools\live_monitor.py --paper
pause
exit /b 0
:no_python
echo Python launcher not found. Install Python or run python tools\live_monitor.py manually.
pause
exit /b 1
