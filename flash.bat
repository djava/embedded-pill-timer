@echo off
set MSYSTEM=
pushd "%~dp0"
call C:\Users\LEORIE~1\esp\esp-idf\export.bat >nul
if errorlevel 1 exit /b 1
if "%~1"=="" (
    idf.py flash
) else (
    idf.py -p %1 flash
)
set EC=%ERRORLEVEL%
popd
exit /b %EC%
