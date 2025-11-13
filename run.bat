@echo off

set CMD=%1
if "%CMD%"=="" (
    echo Usage: run.bat [rel^|dbg]
    echo.
    echo Commands:
    echo   rel - Build and run release version
    echo   dbg - Build and run with debugger
    exit /b 1
)

if "%CMD%"=="rel" goto build_rel
if "%CMD%"=="dbg" goto build_dbg
goto unknown_cmd

:build_rel
    echo ========================================
    echo Building platform...
    echo ========================================
    call build_platform.bat rel
    if errorlevel 1 exit /b 1
    
    echo.
    echo ========================================
    echo Building app DLL...
    echo ========================================
    call build_app.bat rel
    if errorlevel 1 exit /b 1
    
    echo.
    echo ========================================
    echo Starting platform...
    echo ========================================
    echo Press F5 to hot reload after rebuilding app.dll
    echo Press ESC to quit
    echo ========================================
    pushd .\build
    platform.exe
    popd
    exit /b 0

:build_dbg
    echo ========================================
    echo Building platform debug...
    echo ========================================
    call build_platform.bat dbg
    if errorlevel 1 exit /b 1
    
    echo.
    echo ========================================
    echo Building app DLL debug...
    echo ========================================
    call build_app.bat dbg
    if errorlevel 1 exit /b 1
    
    echo.
    echo ========================================
    echo Launching debugger...
    echo ========================================
    pushd .\build
    start "" "raddbg.exe" platform.exe
    popd
    exit /b 0

:unknown_cmd
    echo Unknown command: %CMD%
    echo Usage: run.bat [rel^|dbg]
    exit /b 1