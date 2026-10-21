#pragma once

#include "CanvasTool.h"
#include "core/history/CommandStack.h"
#include "core/history/DocumentCommands.h"
#include "core/layers/Layer.h"
#include "localization/i18nservice.h"
#include <QImage>
#include <QString>

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
