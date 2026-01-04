@echo off
setlocal enabledelayedexpansion

if not exist build mkdir build

set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=rel

set USE_CUDA=%2
if "%USE_CUDA%"=="" set USE_CUDA=yes

call "D:\Programming\Software\msvc\setup_x64.bat" x64

pushd .\build

if "%USE_CUDA%"=="yes" (
    echo Building with CUDA support...
    
    :: Check if nvcc is available
    where nvcc >nul 2>nul
    if errorlevel 1 (
        echo CUDA toolkit not found! Falling back to CPU build.
        goto cpu_build
    )
    
    echo Step 1: Compiling CUDA kernel...
    
    :: GTX 1060 is Pascal architecture (sm_61)
    if "%BUILD_TYPE%"=="rel" (
        nvcc -allow-unsupported-compiler -c -O3 -use_fast_math -arch=sm_61 ^
             -Xcompiler="/MD /nologo" ^
             ..\mandelbrot_gpu.cu -o mandelbrot_gpu.obj
    ) else (
        nvcc -allow-unsupported-compiler -c -G -g -arch=sm_61 ^
             -Xcompiler="/MD /nologo" ^
             ..\mandelbrot_gpu.cu -o mandelbrot_gpu.obj
    )
    
    if errorlevel 1 (
        echo CUDA kernel compilation failed!
        goto cpu_build
    )
    
    echo Step 2: Compiling C application...
    
    :: Compile C code
    if "%BUILD_TYPE%"=="rel" (
        cl /Zi /EHsc /D_AMD64_ /fp:fast /W4 /MD /nologo /arch:AVX /DBUILD_DLL /DUSE_CUDA /O2 ^
           /I..\include /I..\external\include /I"%CUDA_PATH%\include" ^
           /c ..\app.c /Foapp.obj
    ) else (
        cl /Zi /EHsc /D_AMD64_ /fp:fast /W4 /MD /nologo /arch:AVX /DBUILD_DLL /DUSE_CUDA ^
           /I..\include /I..\external\include /I"%CUDA_PATH%\include" ^
           /c ..\app.c /Foapp.obj
    )
    
    if errorlevel 1 (
        echo C compilation failed!
        goto build_failed
    )
    
    echo Step 3: Linking DLL...
    
    :: Link everything together
    link /DLL /OUT:app.dll ^
         app.obj mandelbrot_gpu.obj ^
         /LIBPATH:..\external\lib ^
         /LIBPATH:"%CUDA_PATH%\lib\x64" ^
         cudart.lib ^
         /NOLOGO
    
    if errorlevel 1 (
        echo Linking failed!
        goto build_failed
    )
    
    goto build_success
)

:cpu_build
echo Building with CPU only...

if "%BUILD_TYPE%"=="rel" (
    echo Building app DLL release CPU only...
    cl /Zi /EHsc /D_AMD64_ /fp:fast /W4 /MD /nologo /arch:AVX /DBUILD_DLL /O2 ^
       /I..\include /I..\external\include ^
       ..\app.c /Fe:app.dll /link /DLL /LIBPATH:..\external\lib
) else (
    echo Building app DLL debug CPU only...
    cl /Zi /EHsc /D_AMD64_ /fp:fast /W4 /MD /nologo /arch:AVX /DBUILD_DLL ^
       /I..\include /I..\external\include ^
       ..\app.c /Fe:app.dll /link /DLL /LIBPATH:..\external\lib
)

if errorlevel 1 goto build_failed
goto build_success

:build_failed
echo Build failed!
popd
exit /b 1

:build_success
echo Build successful!
popd
exit /b 0