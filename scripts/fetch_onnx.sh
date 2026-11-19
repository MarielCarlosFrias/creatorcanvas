#!/usr/bin/env bash
set -euo pipefail

# Script to set up ONNX Runtime 1.20.1 and default AI models for CreatorCanvas

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ONNX_DIR="${PROJECT_ROOT}/3rdparty/onnxruntime"
MODELS_DIR="${PROJECT_ROOT}/resources/models"

echo "==> Setting up ONNX Runtime 1.20.1..."
if [ -f "${ONNX_DIR}/lib/libonnxruntime.so" ]; then
    echo "    ONNX Runtime already installed at: ${ONNX_DIR}"
else
    mkdir -p "${PROJECT_ROOT}/3rdparty"
    TEMP_DIR="$(mktemp -d)"
    trap 'rm -rf "${TEMP_DIR}"' EXIT

    echo "    Downloading ONNX Runtime 1.20.1 for Linux x64..."
    curl -fSL --retry 3 "https://github.com/microsoft/onnxruntime/releases/download/v1.20.1/onnxruntime-linux-x64-1.20.1.tgz" -o "${TEMP_DIR}/ort.tgz"
    
    echo "    Extracting to 3rdparty/onnxruntime..."
    tar xzf "${TEMP_DIR}/ort.tgz" -C "${TEMP_DIR}"
    rm -rf "${ONNX_DIR}"
    mv "${TEMP_DIR}/onnxruntime-linux-x64-1.20.1" "${ONNX_DIR}"
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
