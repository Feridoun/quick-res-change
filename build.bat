@echo off
REM Build res_toggle.exe with MSVC. Run from a normal cmd; this sets up the
REM Visual Studio environment automatically.
setlocal

set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (
    echo Could not find vcvars64.bat at:
    echo   %VCVARS%
    echo Edit build.bat to point at your Visual Studio install.
    exit /b 1
)

call "%VCVARS%" >nul

echo Compiling resources...
rc /nologo /fo res_toggle.res res_toggle.rc
if errorlevel 1 exit /b 1

echo Compiling app...
cl /nologo /W3 /O2 /EHsc /DUNICODE /D_UNICODE res_toggle.cpp res_toggle.res ^
   /Fe:res_toggle.exe /link /SUBSYSTEM:WINDOWS ^
   user32.lib shell32.lib shlwapi.lib gdi32.lib
if errorlevel 1 exit /b 1

del /q res_toggle.obj res_toggle.res 2>nul
echo.
echo Done: res_toggle.exe
endlocal
