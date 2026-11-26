#pragma once

#include "CanvasTool.h"
#include "core/history/CommandStack.h"
#include "core/history/DocumentCommands.h"
#include "core/geometry/LayerTransform.h"
#include "core/layers/Layer.h"
#include "core/selection/SelectionOperations.h"
#include "localization/i18nservice.h"
#include <QImage>
#include <QString>
#include <QTransform>

namespace cc {

/// Locks efetivos da camada ativa (união com grupos ancestrais — Photocraft
/// locks_at: trancar um grupo tranca todo o conteúdo). Usado pelas
/// ferramentas antes de qualquer mutação de pixels.
inline LayerLocks effectiveLocksOf(const ToolContext& ctx)
{
    if (!ctx.document || ctx.selectedLayerId.isNull())
        return {};
    return ctx.document->effectiveLocks(ctx.selectedLayerId);
}

/// Transformação completa de uma camada para o espaço do documento. Ferramentas
/// raster não podem usar apenas layer.transform: a camada pode estar dentro de
/// vários grupos transformados.
inline QTransform layerToDocumentTransform(const Document& document, const Layer& layer)
{
    return layerToDocumentMatrix(document, layer);
}

inline QPointF documentToLayerPoint(const Document& document, const Layer& layer,
                                    const QPointF& documentPoint)
{
    bool invertible = false;
    const QTransform inverse = layerToDocumentTransform(document, layer).inverted(&invertible);
    return invertible ? inverse.map(documentPoint) : documentPoint;
}

/// Encontra a imagem visível mais acima sob um ponto, atravessando grupos.
/// Isto mantém ferramentas raster coerentes com a seleção do canvas quando
/// há pastas transformadas, em vez de inspecionar apenas a raiz do documento.
inline ImageLayer* topmostImageLayerAt(const ToolContext& ctx, const QPointF& documentPoint)
{
    if (!ctx.document || !ctx.document->rootGroup())
        return nullptr;
    const auto visit = [&](const auto& self, GroupLayer* group) -> ImageLayer* {
        for (auto it = group->children.rbegin(); it != group->children.rend(); ++it) {
            Layer* candidate = it->get();
            if (!candidate || !candidate->visible)
                continue;
            if (candidate->type() == LayerType::Group) {
                if (ImageLayer* found = self(self, static_cast<GroupLayer*>(candidate)))
                    return found;
                continue;
            }
            if (candidate->type() != LayerType::Image)
                continue;
            const QPointF local = documentToLayerPoint(*ctx.document, *candidate, documentPoint);
            if (candidate->contentBounds().contains(local))
                return static_cast<ImageLayer*>(candidate);
        }
        return nullptr;
    };
    return visit(visit, ctx.document->rootGroup());
}

/// Mantém intactos os pixels fora da seleção ativa. É executado no fim do
/// gesto, uma única vez, para não varrer imagens grandes a cada mouse move.
inline QImage constrainToPixelSelection(const ToolContext& ctx, const QImage& original,
                                        const QImage& edited)
{
    if (!ctx.document)
        return edited;
    return SelectionOperations::applySelectionMask(original, edited,
                                                   ctx.document->pixelSelection());
}

/// Mensagem padrão quando a camada está trancada para pixels.
inline void reportPixelsLocked(const ToolContext& ctx)
{
    if (ctx.requestStatusMessage) {
        ctx.requestStatusMessage(
            ctx.i18n ? ctx.i18n->t("editor", "canvas.status.layerLocked")
                     : QStringLiteral("The layer is locked (image pixels)."));
    }
}

/// Commita um bitmap editado de volta na camada de imagem ativa:
/// cria o asset, limpa a prévia de overlay e registra UM passo de undo
/// (ModifyImageLayerCommand). Sem histórico, aplica direto — a edição
/// nunca pode se perder.
inline void commitImageBitmap(const ToolContext& ctx, const LayerId& layerId,
                              const LayerId& oldAssetId, int oldWidth, int oldHeight,
                              const AffineTransform& oldTransform,
                              const QImage& newImage, const QString& commandName)
{
    if (!ctx.document || layerId.isNull() || newImage.isNull())
        return;
    Layer* layer = ctx.document->findLayer(layerId);
    if (!layer || layer->type() != LayerType::Image)
        return;
    auto* img = static_cast<ImageLayer*>(layer);

    // Cria o asset ANTES de limpar a prévia: se a criação falhar, a
    // prévia continua na tela em vez de a edição sumir sem aviso.
    const LayerId newAssetId = ctx.document->assets().addImage(newImage);
    if (newAssetId.isNull())
        return;

    if (ctx.clearLayerPreview)
        ctx.clearLayerPreview();

    if (ctx.history) {
        ctx.history->execute(std::make_unique<ModifyImageLayerCommand>(
            *ctx.document, layerId,
            oldAssetId, oldWidth, oldHeight, oldTransform,
            newAssetId, img->naturalWidth, img->naturalHeight, layer->transform,
            commandName));
    } else {
        ctx.document->setImageLayerAsset(layerId, newAssetId,
                                         img->naturalWidth, img->naturalHeight);
    }
}

} // namespace cc
