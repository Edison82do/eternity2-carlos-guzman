@echo off
rem Abre el panel del sistema de uniones (doble clic).
cd /d "%~dp0"
where pythonw >nul 2>nul
if %errorlevel%==0 (
    start "" pythonw ventana_uniones.py
) else (
    python ventana_uniones.py
    if errorlevel 1 pause
)
