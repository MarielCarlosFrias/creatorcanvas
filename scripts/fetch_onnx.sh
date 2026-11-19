#!/usr/bin/env bash
set -euo pipefail

# Script to set up ONNX Runtime 1.32.0 and default AI models for CreatorCanvas

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ONNX_DIR="${PROJECT_ROOT}/3rdparty/onnxruntime"
MODELS_DIR="${PROJECT_ROOT}/resources/models"

version="1.32.0"
echo "==> Setting up ONNX Runtime ${version}..."
if [ -f "${ONNX_DIR}/lib/libonnxruntime.so" ]; then
    echo "    ONNX Runtime already installed at: ${ONNX_DIR}"
else
    mkdir -p "${PROJECT_ROOT}/3rdparty"
    TEMP_DIR="$(mktemp -d)"
    trap 'rm -rf "${TEMP_DIR}"' EXIT

    echo "    Downloading ONNX Runtime ${version} for Linux x64..."
    archive="onnxruntime-linux-x64-${version}.tgz"
    urls=("https://sourceforge.net/projects/onnx-runtime.mirror/files/v${version}/${archive}/download" "https://github.com/microsoft/onnxruntime/releases/download/v${version}/${archive}")
    downloaded=false
    for url in "${urls[@]}"; do
        if curl -fSL --retry 3 --retry-all-errors --connect-timeout 20 "$url" -o "${TEMP_DIR}/ort.tgz" && tar tzf "${TEMP_DIR}/ort.tgz" >/dev/null 2>&1; then downloaded=true; break; fi
    done
    if [ "$downloaded" != true ]; then echo "Unable to download ONNX Runtime" >&2; exit 1; fi
    
    echo "    Extracting to 3rdparty/onnxruntime..."
    tar xzf "${TEMP_DIR}/ort.tgz" -C "${TEMP_DIR}"
    rm -rf "${ONNX_DIR}"
    mv "${TEMP_DIR}/onnxruntime-linux-x64-${version}" "${ONNX_DIR}"
    echo "    ONNX Runtime installed successfully."
fi

mkdir -p "${MODELS_DIR}"
echo "==> Checking default AI models..."
if [ ! -f "${MODELS_DIR}/u2netp.onnx" ]; then
    echo "    Downloading default background removal model (u2netp.onnx)..."
    curl -sSL "https://github.com/danielgatis/rembg/releases/download/v0.0.0/u2netp.onnx" -o "${MODELS_DIR}/u2netp.onnx" || {
        echo "    Warning: Could not download u2netp.onnx automatically. You can place it manually in resources/models/."
    }
fi

echo "==> Setup complete! You can now configure and build with CMake."
