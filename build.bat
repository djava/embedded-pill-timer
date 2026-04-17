@echo off
set MSYSTEM=
pushd "%~dp0"
call C:\Users\LEORIE~1\esp\esp-idf\export.bat
if errorlevel 1 exit /b 1
idf.py set-target esp32c3
if errorlevel 1 exit /b 1
idf.py build
set EC=%ERRORLEVEL%
popd
exit /b %EC%
