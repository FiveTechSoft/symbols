@echo off
set PATH=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64;%PATH%
set PATH=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.8\bin;%PATH%

echo Compilando train_cuda_full.cu ...
nvcc -allow-unsupported-compiler -O3 -o train_cuda_full.exe train_cuda_full.cu

if %errorlevel% equ 0 (
    echo.
    echo Compilacion exitosa!
    echo.
    echo Ejecutando entrenamiento...
    train_cuda_full.exe harbour_fwh_v3.jsonl 5000 20
) else (
    echo Error en compilacion
)
