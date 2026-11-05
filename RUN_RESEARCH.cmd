@echo off
setlocal
cd /d "%~dp0"
if not exist bin\pairlab.exe goto missing
set "PL_RUN=runs\run_%RANDOM%_%RANDOM%"
bin\pairlab.exe --data data\real --config config\default.json --out "%PL_RUN%" --stress
if errorlevel 1 goto failed
start "" "%CD%\%PL_RUN%\report.html"
echo Research completed. Results: %PL_RUN%
pause
exit /b 0
:missing
echo Run build_windows.cmd first.
:failed
echo Run failed. Read the error above; previous reports remain available.
pause
exit /b 1
