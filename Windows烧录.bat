@echo off
setlocal EnableExtensions DisableDelayedExpansion
chcp 65001 >nul
set "PYTHONUTF8=1"
set "PYTHONIOENCODING=utf-8"
pushd "%~dp0"
if errorlevel 1 exit /b 1

python -c "import sys; assert sys.version_info >= (3, 10)" >nul 2>&1
if not errorlevel 1 goto use_python
py -3 -c "import sys; assert sys.version_info >= (3, 10)" >nul 2>&1
if not errorlevel 1 goto use_launcher
echo Python 3.10+ was not found. Install Python or open an activated ESP-IDF terminal.
set "PET_RESULT=1"
goto finish

:use_python
python "%~dp0tools\windows_flash.py" %*
set "PET_RESULT=%errorlevel%"
goto finish

:use_launcher
py -3 "%~dp0tools\windows_flash.py" %*
set "PET_RESULT=%errorlevel%"

:finish
echo.
echo Exit code: %PET_RESULT%
popd
if not defined PET_NO_PAUSE pause
exit /b %PET_RESULT%
