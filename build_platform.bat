@echo off

if not exist build mkdir build
copy ".\external\lib\*.dll" ".\build\" >nul 2>&1

:: Set environment vars for MSVC compiler
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

set CFLAGS=/Zi /EHsc /D_AMD64_ /fp:fast /W4 /MD /nologo /utf-8 /std:clatest /arch:AVX
set L_FLAGS=/SUBSYSTEM:CONSOLE
set SRC=..\platform.c ..\external\src\glad.c
set INCLUDE_DIRS=/I..\include /I..\external\include\
set LIBRARY_DIRS=/LIBPATH:..\external\lib\
set LIBRARIES=opengl32.lib glfw3.lib glew32.lib UxTheme.lib Dwmapi.lib user32.lib gdi32.lib shell32.lib kernel32.lib

set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=rel

pushd .\build

if "%BUILD_TYPE%"=="rel" (
    echo Building platform release...
    cl %CFLAGS% /O2 %INCLUDE_DIRS% %SRC% /Fe:platform.exe /link %LIBRARY_DIRS% %LIBRARIES% %L_FLAGS%
    if errorlevel 1 (
        echo Platform build failed!
        popd
        exit /b 1
    )
    echo Platform built successfully!
)

if "%BUILD_TYPE%"=="dbg" (
    echo Building platform debug...
    cl %CFLAGS% /fsanitize=address %INCLUDE_DIRS% %SRC% /Fe:platform.exe /link %LIBRARY_DIRS% %LIBRARIES% %L_FLAGS%
    if errorlevel 1 (
        echo Platform debug build failed!
        popd
        exit /b 1
    )
    echo Platform debug build successful!
)

popd