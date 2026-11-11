#pragma once

#include "BaseCommands.h"
#include "core/history/CommandStack.h"
#include "core/image/AdjustmentEngine.h"
#include "core/layers/Layer.h"
#include "core/commands/LayerPropertyCommands.h"

#include <optional>

namespace cc {

/// Cria uma camada de ajuste, aplica os parâmetros e (opcionalmente) liga-a
/// a uma camada de imagem — um único passo de histórico. O undo desfaz o
/// vínculo e remove a camada criada.
class CreateAdjustmentLayerCommand final : public Command
{
public:
    CreateAdjustmentLayerCommand(Document& doc, AdjustmentType type,
                                 AdjustmentParams params,
                                 std::optional<LayerId> linkTo,
                                 QString name = QStringLiteral("adjustment.create"))
        : Command(std::move(name))
        , m_doc(&doc)
        , m_type(type)
        , m_params(std::move(params))
        , m_linkTo(std::move(linkTo))
    {
    }

    void redo() override
    {
        if (m_doc == nullptr || m_done)
            return;
        m_createdId = m_doc->createAdjustmentLayer(m_type, m_linkTo);
        if (m_createdId.isNull())
            return;
        if (!m_params.empty())
            m_doc->setAdjustmentParameters(m_createdId, m_params);
        // O vínculo vive na CAMADA DE IMAGEM (ImageLayer::adjustmentLayerId),
        // não na camada de ajuste — sem isto o renderer nunca a aplicaria.
        if (m_linkTo)
            m_doc->setLayerAdjustmentLayerId(*m_linkTo, m_createdId);
        m_done = true;
    }

    void undo() override
    {
        if (m_doc == nullptr || !m_done)
            return;
        if (m_linkTo)
            m_doc->setLayerAdjustmentLayerId(*m_linkTo, std::nullopt);
        m_doc->removeLayer(m_createdId);
        m_done = false;
        m_createdId = LayerId{};
    }

    LayerId createdId() const { return m_createdId; }

private:
    Document* m_doc = nullptr;
    AdjustmentType m_type = AdjustmentType::BrightnessContrast;
    AdjustmentParams m_params;
    std::optional<LayerId> m_linkTo;
    LayerId m_createdId;
    bool m_done = false;
};

/// Muda tipo E parâmetros de uma camada de ajuste existente num único passo
/// de histórico (o utilizador raramente quer dois undos para um ajuste).
class UpdateAdjustmentLayerCommand final : public Command
{
public:
    UpdateAdjustmentLayerCommand(Document& doc, const LayerId& id,
                                 AdjustmentType oldKind, AdjustmentType newKind,
                                 AdjustmentParams oldParams, AdjustmentParams newParams,
                                 QString name = QStringLiteral("adjustment.update"))
        : Command(std::move(name))
        , m_doc(&doc)
        , m_id(id)
        , m_oldKind(oldKind)
        , m_newKind(newKind)
        , m_oldParams(std::move(oldParams))
        , m_newParams(std::move(newParams))
    {
    }

    void redo() override
    {
        if (m_doc == nullptr || !m_doc->findAdjustmentLayer(m_id))
            return;
        m_doc->setAdjustmentKind(m_id, m_newKind);
        m_doc->setAdjustmentParameters(m_id, m_newParams);
    }

    void undo() override
    {
        if (m_doc == nullptr || !m_doc->findAdjustmentLayer(m_id))
            return;
        m_doc->setAdjustmentKind(m_id, m_oldKind);
        m_doc->setAdjustmentParameters(m_id, m_oldParams);
    }

private:
    Document* m_doc = nullptr;
    LayerId m_id;
    AdjustmentType m_oldKind = AdjustmentType::BrightnessContrast;
    AdjustmentType m_newKind = AdjustmentType::BrightnessContrast;
    AdjustmentParams m_oldParams;
    AdjustmentParams m_newParams;
};

/// Actualiza só os parâmetros de uma camada de ajuste existente (usado por
/// arrastos de slider ao vivo, que fazem merge entre eventos).
using SetAdjustmentParametersCommand =
    LayerPropertyCommand<std::vector<std::pair<QString, double>>>;

/// Política única de aplicação de um ajuste a uma camada de imagem, usada
/// pelo diálogo de ajustes e pelo inspector:
///
///  - nonDestructive = true: cria (ou reutiliza) uma camada de ajuste ligada
///    à imagem — o render aplica em tempo real, nada é cozido;
///  - nonDestructive = false: aplica os parâmetros ao bitmap e troca o asset
///    (comportamento dos filtros antigos do inspector).
///
/// Devolve true se algo mudou. Com CommandStack nulo a edição é aplicada
/// directa (a edição nunca se perde — mesmo padrão dos outros comandos).
inline bool executeApplyAdjustment(Document& doc, CommandStack* history,
                                   const LayerId& imageLayerId,
                                   AdjustmentType type,
                                   const AdjustmentParams& params,
                                   bool nonDestructive,
                                   const QString& name = QStringLiteral("adjustment.apply"))
{
    Layer* layer = doc.findLayer(imageLayerId);
    if (!layer || layer->type() != LayerType::Image)
        return false;
    auto* img = static_cast<ImageLayer*>(layer);

    if (nonDestructive) {
        LayerId adjId = img->adjustmentLayerId.value_or(LayerId{});
        const bool hasLinked = !adjId.isNull() && doc.findAdjustmentLayer(adjId) != nullptr;
        if (hasLinked) {
            // Reutiliza a camada ligada: tipo novo + parâmetros novos, um undo.
            const AdjustmentLayer* adj = doc.findAdjustmentLayer(adjId);
            const AdjustmentType oldKind = adj ? adj->kind : type;
            const AdjustmentParams oldParams = adj ? adj->parameters : AdjustmentParams{};
            if (history) {
                history->execute(std::make_unique<UpdateAdjustmentLayerCommand>(
                    doc, adjId, oldKind, type, oldParams, params, name));
            } else {
                doc.setAdjustmentKind(adjId, type);
                doc.setAdjustmentParameters(adjId, params);
            }
            return true;
        }
        if (history) {
            auto cmd = std::make_unique<CreateAdjustmentLayerCommand>(
                doc, type, params, imageLayerId, name);
            history->execute(std::move(cmd));
        } else {
            adjId = doc.createAdjustmentLayer(type, imageLayerId);
            if (adjId.isNull())
                return false;
            if (!params.empty())
                doc.setAdjustmentParameters(adjId, params);
            doc.setLayerAdjustmentLayerId(imageLayerId, adjId);
        }
        return true;
    }

    // Caminho destrutivo: resultados cozidos num novo asset.
    const QImage src = doc.assets().decodedImage(img->assetId);
    if (src.isNull())
        return false;
    const QImage result = applyAdjustmentParameters(src, type, params);
    if (result.isNull() || result.size() != src.size())
        return false;
    const LayerId newAsset = doc.assets().addImage(result, QStringLiteral("png"));
    const int newW = result.width();
    const int newH = result.height();
    if (history) {
        history->execute(std::make_unique<SetLayerImageAssetCommand>(
            doc, imageLayerId, img->assetId, img->naturalWidth, img->naturalHeight,
            newAsset, newW, newH));
    } else {
        doc.setImageLayerAsset(imageLayerId, newAsset, newW, newH);
    }
    return true;
}

} // namespace cc
