#pragma once

#include "core/Document.h"

#include <QImage>
#include <QSize>
#include <QString>

#include <vector>

namespace cc {

/// Algoritmo usado tanto nos assets como nas máscaras. Bilinear usa a
/// reamostragem suave do Qt; Bicubic usa uma kernel Catmull-Rom separada por
/// canal e trabalha em alfa pré-multiplicado para evitar halos.
enum class ResampleMethod {
    NearestNeighbor,
    Bilinear,
    Bicubic
};

struct ImageResizeOptions {
    QSize size;
    ResampleMethod method = ResampleMethod::Bicubic;
};

/// Operação de Image Size: altera as dimensões do documento E reamostra o
/// conteúdo. Não confundir com Document::setCanvasSize/ResizeCanvasCommand,
/// que apenas recortam ou acrescentam canvas.
class ImageResizer final
{
public:
    struct LayerState {
        LayerId id;
        LayerType type = LayerType::Group;
        AffineTransform transform;
        QImage vectorMask;

        // Preenchido apenas para ImageLayer.
        LayerId assetId;
        int naturalWidth = 0;
        int naturalHeight = 0;
        QImage alphaMask;
    };

    /// Estado necessário para undo/redo sem uma segunda reamostragem. Os
    /// assets continuam no AssetStore e o comando mantém os seus ids vivos.
    struct Snapshot {
        int width = 0;
        int height = 0;
        std::vector<LayerState> layers;
        SelectionState selection;
        std::vector<Guide> guides;
        std::vector<WebSlice> webSlices;
    };

    static QImage resample(const QImage& source, const QSize& targetSize,
                           ResampleMethod method);
    static QImage resampleMask(const QImage& source, const QSize& targetSize,
                               ResampleMethod method);

    static Snapshot capture(const Document& document);
    static void restore(Document& document, const Snapshot& snapshot);

    /// Devolve false sem alterar camadas/seleções quando o pedido é inválido,
    /// é um no-op, ou algum asset não pode ser descodificado.
    static bool resize(Document& document, const ImageResizeOptions& options,
                       QString* error = nullptr);
    static bool resize(Document& document, const QSize& newSize,
                       ResampleMethod method = ResampleMethod::Bicubic,
                       QString* error = nullptr)
    {
        return resize(document, ImageResizeOptions{newSize, method}, error);
    }
    static bool resize(Document& document, int newWidth, int newHeight,
                       ResampleMethod method = ResampleMethod::Bicubic,
                       QString* error = nullptr)
    {
        return resize(document, QSize(newWidth, newHeight), method, error);
    }
};

} // namespace cc
