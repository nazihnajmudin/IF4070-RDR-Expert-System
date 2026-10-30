@echo off
rem Build script for native Windows using MinGW-w64 gcc (w64devkit or MSYS2 mingw64).
rem
rem Usage
rem   scripts\build.bat           build GUI and CLI
rem   scripts\build.bat clean     remove build\windows
rem
rem Output
rem   build\windows\rdr-expert.exe   GUI, double click to run
rem   build\windows\rdr-cli.exe      command line runner
rem
rem Requirements
rem   gcc from MinGW-w64 available in PATH. Make is not required.

setlocal EnableDelayedExpansion
cd /d "%~dp0\.."

set OUT=build\windows
set OBJ=%OUT%\obj
set GEN=%OUT%\gen
set HOST=build\host

rem Clean Target
if /i "%~1"=="clean" (
    if exist "%OUT%" rmdir /s /q "%OUT%"
    if exist "%HOST%" rmdir /s /q "%HOST%"
    echo Removed %OUT%
    exit /b 0
)

rem Tool Check
where gcc >nul 2>nul
if errorlevel 1 (
    echo error: gcc was not found in PATH.
    echo Install MinGW-w64, for example w64devkit from https://github.com/skeeto/w64devkit,
    echo then run this script from a terminal where gcc is available.
    exit /b 1
)

if not exist "%OBJ%" mkdir "%OBJ%"
if not exist "%GEN%" mkdir "%GEN%"
if not exist "%HOST%" mkdir "%HOST%"

set CFLAGS=-std=c99 -O2 -Wall -Wextra -Iinclude
set GUIFLAGS=-Isrc\gui -Ithird_party\nuklear -Ithird_party\glfw\include -I%GEN%

rem Embedded Resources
echo [1/5] Embedding resources
gcc -std=c99 -O2 -o "%HOST%\embed.exe" tools\embed.c || goto :fail
"%HOST%\embed.exe" third_party\fonts\DejaVuSansMono.ttf font_mono "%GEN%\font_mono.c" || goto :fail
"%HOST%\embed.exe" third_party\fonts\DejaVuSansMono-Bold.ttf font_mono_bold "%GEN%\font_mono_bold.c" || goto :fail
"%HOST%\embed.exe" assets\help\help_id.txt help_text_id "%GEN%\help_text_id.c" || goto :fail
"%HOST%\embed.exe" assets\help\help_en.txt help_text_en "%GEN%\help_text_en.c" || goto :fail

rem Source Lists
set CORE=
for %%f in (src\core\*.c src\app\*.c src\platform\*.c) do set CORE=!CORE! %%f
set GUI=
for %%f in (src\gui\*.c) do set GUI=!GUI! %%f
set GENSRC=
for %%f in (%GEN%\*.c) do set GENSRC=!GENSRC! %%f

rem GLFW Objects
echo [2/5] Compiling GLFW
set GLFWOBJ=
for %%f in (context init input monitor platform vulkan window egl_context osmesa_context null_init null_monitor null_window null_joystick win32_module win32_time win32_thread win32_init win32_joystick win32_monitor win32_window wgl_context) do (
    gcc -std=gnu99 -O2 -w -D_GLFW_WIN32 -c third_party\glfw\src\%%f.c -o "%OBJ%\glfw_%%f.o" || goto :fail
    set GLFWOBJ=!GLFWOBJ! "%OBJ%\glfw_%%f.o"
)

rem Command Line Program
echo [3/5] Building rdr-cli.exe
gcc %CFLAGS% -static -o "%OUT%\rdr-cli.exe" %CORE% src\cli\main_cli.c || goto :fail

rem Graphical Program
echo [4/5] Building rdr-expert.exe
gcc %CFLAGS% %GUIFLAGS% -static -mwindows -o "%OUT%\rdr-expert.exe" %CORE% %GUI% %GENSRC% %GLFWOBJ% -lopengl32 -lgdi32 -luser32 -lshell32 || goto :fail

echo [5/5] Done
echo   %OUT%\rdr-expert.exe
echo   %OUT%\rdr-cli.exe
exit /b 0

:fail
echo Build failed.
exit /b 1
