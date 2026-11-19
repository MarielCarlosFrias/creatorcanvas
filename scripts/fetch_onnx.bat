@echo off
setlocal enabledelayedexpansion

REM Script to set up ONNX Runtime 1.32.0 for Windows x64

set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%.."
set "ONNX_DIR=%PROJECT_ROOT%\3rdparty\onnxruntime"
set "MODELS_DIR=%PROJECT_ROOT%\resources\models"

set "ORT_VERSION=1.32.0"
echo ==> Setting up ONNX Runtime %ORT_VERSION% for Windows...
if exist "%ONNX_DIR%\lib\onnxruntime.dll" (
    echo     ONNX Runtime already installed at: %ONNX_DIR%
) else (
    if not exist "%PROJECT_ROOT%\3rdparty" mkdir "%PROJECT_ROOT%\3rdparty"
    set "ZIP_FILE=%TEMP%\ort.zip"
    set "EXTRACT_DIR=%TEMP%\ort_extracted"

    echo     Downloading ONNX Runtime %ORT_VERSION% for Windows x64...
    powershell -NoProfile -Command "$ErrorActionPreference='Stop'; $v='%ORT_VERSION%'; $a='onnxruntime-win-x64-'+$v+'.zip'; $urls=@('https://sourceforge.net/projects/onnx-runtime.mirror/files/v'+$v+'/'+$a+'/download','https://github.com/microsoft/onnxruntime/releases/download/v'+$v+'/'+$a); foreach($u in $urls){try{Invoke-WebRequest -Uri $u -OutFile '%ZIP_FILE%' -UseBasicParsing; break}catch{if($u -eq $urls[-1]){throw}}}"
    
    echo     Extracting to 3rdparty\onnxruntime...
    powershell -Command "Expand-Archive -Path '%ZIP_FILE%' -DestinationPath '%EXTRACT_DIR%' -Force"
    if exist "%ONNX_DIR%" rmdir /s /q "%ONNX_DIR%"
    move "%EXTRACT_DIR%\onnxruntime-win-x64-%ORT_VERSION%" "%ONNX_DIR%"
    del /f /q "%ZIP_FILE%"
    rmdir /s /q "%EXTRACT_DIR%"
    echo     ONNX Runtime installed successfully.
)

if not exist "%MODELS_DIR%" mkdir "%MODELS_DIR%"
if not exist "%MODELS_DIR%\u2netp.onnx" (
    echo     Downloading default background removal model (u2netp.onnx)...
    powershell -Command "Invoke-WebRequest -Uri 'https://github.com/danielgatis/rembg/releases/download/v0.0.0/u2netp.onnx' -OutFile '%MODELS_DIR%\u2netp.onnx'"
)

echo ==> Setup complete! You can now configure and build with CMake.
