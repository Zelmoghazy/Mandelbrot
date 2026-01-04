@echo off

if not exist build mkdir build

:: Set environment vars for MSVC compiler
call "D:\Programming\Software\msvc\setup_x64.bat" x64

set CFLAGS=/Zi /EHsc /D_AMD64_ /fp:fast /W4 /MD /nologo /utf-8 /std:clatest /arch:AVX /DBUILD_DLL
set L_FLAGS=/DLL
set SRC=..\app.c
set INCLUDE_DIRS=/I..\include /I..\external\include\
set LIBRARY_DIRS=/LIBPATH:..\external\lib\

set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=rel

pushd .\build

if "%BUILD_TYPE%"=="rel" (
    echo Building app DLL release...
    cl %CFLAGS% /O2 %INCLUDE_DIRS% %SRC% /Fe:app.dll /link %L_FLAGS% %LIBRARY_DIRS%
    if errorlevel 1 (
        goto :build_failed
    )
    goto :build_success
)

if "%BUILD_TYPE%"=="dbg" (
    echo Building app DLL debug...
    cl %CFLAGS% %INCLUDE_DIRS% %SRC% /Fe:app.dll /link %L_FLAGS% %LIBRARY_DIRS%
    if errorlevel 1 (
        goto :build_failed
    )
    goto :build_success
)

:build_failed
echo App DLL build failed!
popd
exit /b 1

:build_success
echo App DLL build successful!
popd
exit /b 0