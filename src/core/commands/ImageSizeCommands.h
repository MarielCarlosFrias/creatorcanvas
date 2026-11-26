#pragma once

#include "core/history/Command.h"
#include "services/ImageResizer.h"

namespace cc {

/// Image Size/Resample como um único passo de histórico. O primeiro redo
/// calcula o resultado; undo/redo seguintes restauram snapshots, evitando
/// reamostragem cumulativa e preservando ids de assets partilhados.
class ResizeImageCommand final : public Command
{
public:
    ResizeImageCommand(Document& document, const ImageResizeOptions& options)
        : Command(QStringLiteral("image.resize"))
        , m_document(&document)
        , m_options(options)
        , m_before(ImageResizer::capture(document))
    {
    }

    ResizeImageCommand(Document& document, int newWidth, int newHeight,
                       ResampleMethod method = ResampleMethod::Bicubic)
        : ResizeImageCommand(document,
                             ImageResizeOptions{QSize(newWidth, newHeight), method})
    {
    }

    void redo() override
    {
        if (m_hasAfter) {
            ImageResizer::restore(*m_document, m_after);
            return;
        }
        if (!ImageResizer::resize(*m_document, m_options, &m_error))
            return;
        m_after = ImageResizer::capture(*m_document);
        m_hasAfter = true;
    }

    void undo() override
    {
        if (m_hasAfter)
            ImageResizer::restore(*m_document, m_before);
    }

    void collectAssetIds(QSet<LayerId>& out) const override
    {
        collectSnapshotAssets(m_before, out);
        if (m_hasAfter)
            collectSnapshotAssets(m_after, out);
    }

    bool wasApplied() const { return m_hasAfter; }
    const QString& error() const { return m_error; }

private:
    static void collectSnapshotAssets(const ImageResizer::Snapshot& snapshot,
                                      QSet<LayerId>& out)
    {
        for (const ImageResizer::LayerState& layer : snapshot.layers) {
            if (layer.type == LayerType::Image && !layer.assetId.isNull())
                out.insert(layer.assetId);
        }
    }

    Document* m_document;
    ImageResizeOptions m_options;
    ImageResizer::Snapshot m_before;
    ImageResizer::Snapshot m_after;
    bool m_hasAfter = false;
    QString m_error;
};

// Nome alternativo explícito para integrações que preferem alinhar a classe
// com o título do diálogo. A operação continua a ser "image.resize".
using ImageSizeCommand = ResizeImageCommand;

} // namespace cc
