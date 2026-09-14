# Builds a portable Windows ZIP. Run from a VS/Qt developer prompt:
#   powershell -File packaging\windows\build_portable.ps1 -BuildDir build\windows-release
param([string]$BuildDir = "build\windows-release")

cmake --build $BuildDir --config Release --target creatorcanvas

$OutDir = "packaging\windows\CreatorCanvas-portable"
if (Test-Path $OutDir) { Remove-Item -Recurse -Force $OutDir }
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

Copy-Item "$BuildDir\bin\Release\CreatorCanvas.exe" $OutDir

# Copy ONNX Runtime DLL if available
if (Test-Path "3rdparty\onnxruntime\lib\onnxruntime.dll") {
    Copy-Item "3rdparty\onnxruntime\lib\onnxruntime.dll" $OutDir
} elseif (Test-Path "$BuildDir\bin\Release\onnxruntime.dll") {
    Copy-Item "$BuildDir\bin\Release\onnxruntime.dll" $OutDir
}

# Bundle Qt DLLs + plugins (platforms, styles, imageformats)
windeployqt --release --no-translations --no-system-d3d-compiler `
    --no-opengl-sw --dir $OutDir "$OutDir\CreatorCanvas.exe"

Compress-Archive -Path $OutDir -DestinationPath "CreatorCanvas-portable.zip" -Force
Write-Host ">> ZIP: CreatorCanvas-portable.zip"
