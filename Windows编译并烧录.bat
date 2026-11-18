@echo off
chcp 65001 >nul
call "%~dp0Windows烧录.bat" --build %*
exit /b %errorlevel%
