# Builds a portable Windows ZIP. Run from a VS/Qt developer prompt:
#   powershell -File packaging\windows\build_portable.ps1 -BuildDir build\windows-release
param([string]$BuildDir = "build\windows-release")

# minipaint-standalone is built in-tree (CC_MINIPAINT_SOURCE_DIR defaults to
# 3rdparty/minipaint); building creatorcanvas drags it via a POST_BUILD copy,
# but naming it keeps the intent obvious.
cmake --build $BuildDir --config Release --target creatorcanvas minipaint-standalone

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

# External editor (Image > Edit in MiniPaint): the standalone plus the SFML
# 3 DLLs it links dynamically. Windows resolves DLLs next to the executable,
# so they live in the package root beside CreatorCanvas.exe.
$MiniPaint = "$BuildDir\bin\Release\minipaint-standalone.exe"
if (Test-Path $MiniPaint) {
    Copy-Item $MiniPaint $OutDir
    $sfmlCandidates = @()
    $sfmlCandidates += (Get-ChildItem "$BuildDir\bin\Release\sfml-*.dll" -ErrorAction SilentlyContinue)
    if ($env:SFML_DIR) { $sfmlCandidates += (Get-ChildItem "$env:SFML_DIR\bin\sfml-*.dll" -ErrorAction SilentlyContinue) }
    if ($sfmlCandidates.Count -eq 0) {
        Write-Warning "SFML DLLs not found next to $MiniPaint or in `$env:SFML_DIR - the portable package will miss MiniPaint's runtime. Install SFML 3 and rebuild."
    } else {
        Copy-Item $sfmlCandidates $OutDir
    }
} else {
    Write-Host ">> MiniPaint standalone not built (SFML 3 missing?) - 'Edit in MiniPaint' will be disabled in this package."
}

# Copy default models if available
if (Test-Path "resources\models") {
    New-Item -ItemType Directory -Force -Path "$OutDir\models" | Out-Null
    Copy-Item "resources\models\*.*" "$OutDir\models"
}

# Bundle Qt DLLs + plugins (platforms, styles, imageformats)
windeployqt --release --no-translations --no-system-d3d-compiler `
    --no-opengl-sw --dir $OutDir "$OutDir\CreatorCanvas.exe"

Compress-Archive -Path $OutDir -DestinationPath "CreatorCanvas-portable.zip" -Force
Write-Host ">> ZIP: CreatorCanvas-portable.zip"
