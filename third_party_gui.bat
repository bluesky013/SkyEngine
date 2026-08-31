@echo off
cd /d "%~dp0"
python python\third_party_gui.py %*
if errorlevel 1 pause
