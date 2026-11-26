#pragma once

#include <QString>

namespace cc {

class Document;

/// Exporta o documento como SVG híbrido. Formas básicas (rect/rounded/
/// ellipse/line/polygon), camadas vectoriais (Bézier) e texto simples são
/// escritos como elementos vetoriais; imagens e formas complexas são embebidas
/// como PNG rasterizado em <image>. |scale| multiplica as dimensões de saída,
/// mantendo o viewBox em coordenadas do documento para preservar os vectores.
/// Efeitos de texto/sombra e máscaras de recorte têm suporte limitado.
bool exportDocumentToSvg(const Document& doc, const QString& filePath,
                         double scale, QString* errorMessage = nullptr);

/// Compatibilidade com chamadores anteriores: exporta à escala 1:1.
bool exportDocumentToSvg(const Document& doc, const QString& filePath,
                         QString* errorMessage = nullptr);

} // namespace cc
