@echo off
rem Abre el panel de la version 6 (tarjeta de video).
cd /d "%~dp0"
where pythonw >/dev/null 2>nul
if %errorlevel%==0 (
    start "" pythonw ventana_gpu.py
) else (
    python ventana_gpu.py
    if errorlevel 1 pause
)
