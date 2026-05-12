@echo off
setlocal
echo === Mal4Death Build Script ===
echo.

:: -- Try GCC (w64devkit / MinGW / WinLibs) --
set GCC=
where gcc >nul 2>&1
if not errorlevel 1 ( set GCC=gcc & goto :build_gcc )

for %%P in (
    "C:\w64devkit\bin\gcc.exe"
    "C:\mingw64\bin\gcc.exe"
    "C:\MinGW\bin\gcc.exe"
    "C:\winlibs\bin\gcc.exe"
    "C:\msys64\mingw64\bin\gcc.exe"
    "C:\msys64\ucrt64\bin\gcc.exe"
) do (
    if exist %%P ( set GCC=%%~P & set "GCCBIN=%%~dpP" & goto :build_gcc )
)

:: -- Try MSVC (cl.exe) --
where cl >nul 2>&1 && goto :build_msvc

:: -- No compiler found --
echo ERROR: No C compiler found.
echo.
echo Install w64devkit (standalone GCC, no setup required):
echo   1. Go to: github.com/skeeto/w64devkit/releases
echo   2. Download w64devkit-x86_64-*.zip
echo   3. Extract so you have C:\w64devkit\bin\gcc.exe
echo   4. Close and reopen this window, then run build.bat again.
echo.
goto :end

:: -- Build with GCC --
:build_gcc
if defined GCCBIN set "PATH=%GCCBIN%;%PATH%"
echo Compiler: %GCC%

echo Building malware sample...
"%GCC%" -mwindows tools\malus_sample_src.c -o assets\malus_sample.exe -lkernel32 -luser32
if %ERRORLEVEL% neq 0 ( echo FAILED to build malus_sample.exe & goto :end )

echo Generating keys...
"%GCC%" -o tools\keygen.exe tools\keygen.c
if %ERRORLEVEL% neq 0 ( echo FAILED to compile keygen & goto :end )
tools\keygen.exe
if %ERRORLEVEL% neq 0 ( echo FAILED to run keygen & goto :end )

:: embed icon (windres ships with w64devkit/MinGW)
set RC_OBJ=
windres assets\app.rc -O coff -o assets\app_res.o >nul 2>&1
if not errorlevel 1 ( set RC_OBJ=assets\app_res.o ) else ( echo Note: windres not found, icon not embedded. )

echo Compiling...
set _SRCS=src/main.c src/ui.c src/difficulty.c src/levels.c src/validation.c src/tools.c src/timer.c src/hints.c src/audio.c
"%GCC%" -Wall -std=c11 %_SRCS% %RC_OBJ% -o mal4death.exe -lkernel32 -luser32 -lshell32 -lwinmm
goto :result

:: -- Build with MSVC --
:build_msvc
echo Compiler: cl.exe (MSVC)

echo Building malware sample...
cl /nologo /TC tools\malus_sample_src.c /Fe:assets\malus_sample.exe kernel32.lib user32.lib /link /SUBSYSTEM:WINDOWS
if %ERRORLEVEL% neq 0 ( echo FAILED to build malus_sample.exe & goto :end )

echo Generating keys...
cl /nologo /TC tools\keygen.c /Fe:tools\keygen.exe
if %ERRORLEVEL% neq 0 ( echo FAILED to compile keygen & goto :end )
tools\keygen.exe
if %ERRORLEVEL% neq 0 ( echo FAILED to run keygen & goto :end )

echo Compiling...
cl /nologo /TC /std:c11 /W3 ^
    src/main.c src/ui.c src/difficulty.c src/levels.c ^
    src/validation.c src/tools.c src/timer.c src/hints.c src/audio.c ^
    /Fe:mal4death.exe ^
    kernel32.lib user32.lib shell32.lib winmm.lib
goto :result

:: -- Result --
:result
if %ERRORLEVEL% == 0 (
    echo.
    echo BUILD OK -- run mal4death.exe to play.
    echo Keys saved to keys.txt
) else (
    echo.
    echo BUILD FAILED -- check errors above.
)

:end
endlocal
pause
