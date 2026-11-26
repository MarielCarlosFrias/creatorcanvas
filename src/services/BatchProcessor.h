#pragma once

#include <QByteArray>
#include <QSize>
#include <QString>
#include <QStringList>

namespace cc {

struct BatchImageOptions {
    QStringList operations;
    QSize resizeSize;
    QByteArray outputFormat = QByteArray("png");
    int outputQuality = -1;
    bool overwrite = false;
};

/// Processa um ficheiro de imagem no batch, usando o mesmo importador robusto
/// da UI principal (incluindo PSD) e os exportadores PDF/SVG existentes.
/// |outputPath| pode omitir a extensão; ela é acrescentada a partir do formato.
bool processBatchImageFile(const QString& inputPath, const QString& outputPath,
                           const BatchImageOptions& options,
                           QString* errorMessage = nullptr);

} // namespace cc
