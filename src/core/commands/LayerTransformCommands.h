#pragma once

#include "BaseCommands.h"

namespace cc {

class SetLayerTransformCommand final : public LayerCommand
{
public:
    SetLayerTransformCommand(Document& doc, const LayerId& id,
                             AffineTransform oldValue, AffineTransform newValue)
        : LayerCommand(QStringLiteral("layer.transform"), doc, id, LockRequirement::Position)
        , m_oldValue(std::move(oldValue))
        , m_newValue(std::move(newValue))
    {
    }

    void redo() override
    {
        if (!canExecute())
            return;
        m_doc->setLayerTransform(m_layerId, m_newValue);
    }

    void undo() override
    {
        if (!canExecute())
            return;
        m_doc->setLayerTransform(m_layerId, m_oldValue);
    }

    bool mergeWith(const Command& other) override
    {
        const auto* rhs = dynamic_cast<const SetLayerTransformCommand*>(&other);
        if (!rhs || rhs->m_doc != m_doc || rhs->m_layerId != m_layerId)
            return false;
        m_newValue = rhs->m_newValue;
        return true;
    }

private:
    AffineTransform m_oldValue;
    AffineTransform m_newValue;
};

/// Comando reversível para modificações de conteúdo em camadas de imagem (corte, varinha mágica, tesoura, clone stamp).
/// Preserva o asset anterior e novo, dimensões naturais e transformações (posição/escala) para suporte completo a Desfazer/Refazer.
class ModifyImageLayerCommand final : public LayerCommand
{
public:
    ModifyImageLayerCommand(Document& doc, const LayerId& id,
                            LayerId oldAssetId, int oldWidth, int oldHeight, AffineTransform oldTransform,
                            LayerId newAssetId, int newWidth, int newHeight, AffineTransform newTransform,
                            QString commandName = QStringLiteral("image.modify"),
                            QImage oldAlphaMask = {}, QImage newAlphaMask = {},
                            QImage oldVectorMask = {}, QImage newVectorMask = {})
        : LayerCommand(std::move(commandName), doc, id, LockRequirement::Pixels)
        , m_oldAssetId(oldAssetId)
        , m_oldWidth(oldWidth)
        , m_oldHeight(oldHeight)
        , m_oldTransform(std::move(oldTransform))
        , m_newAssetId(newAssetId)
        , m_newWidth(newWidth)
        , m_newHeight(newHeight)
        , m_newTransform(std::move(newTransform))
        , m_oldAlphaMask(std::move(oldAlphaMask))
        , m_newAlphaMask(std::move(newAlphaMask))
        , m_oldVectorMask(std::move(oldVectorMask))
        , m_newVectorMask(std::move(newVectorMask))
        , m_hasAlphaMasks(!m_oldAlphaMask.isNull() || !m_newAlphaMask.isNull())
        , m_hasVectorMasks(!m_oldVectorMask.isNull() || !m_newVectorMask.isNull())
    {
    }

    bool canExecute() const override
    {
        if (!m_doc || m_layerId.isNull())
            return false;
        const auto locks = m_doc->effectiveLocks(m_layerId);
        if (locks.all || locks.pixels)
            return false;
        if (m_oldTransform != m_newTransform && locks.position)
            return false;
        return true;
    }

    void collectAssetIds(QSet<LayerId>& out) const override
    {
        out.insert(m_oldAssetId);
        out.insert(m_newAssetId);
    }

    void redo() override
    {
        if (!canExecute())
            return;
        m_doc->setImageLayerAsset(m_layerId, m_newAssetId, m_newWidth, m_newHeight);
        if (m_hasAlphaMasks)
            m_doc->setLayerAlphaMask(m_layerId, m_newAlphaMask);
        if (m_hasVectorMasks)
            m_doc->setLayerVectorMask(m_layerId, m_newVectorMask);
        m_doc->setLayerTransform(m_layerId, m_newTransform);
    }

    void undo() override
    {
        if (!canExecute())
            return;
        m_doc->setImageLayerAsset(m_layerId, m_oldAssetId, m_oldWidth, m_oldHeight);
        if (m_hasAlphaMasks)
            m_doc->setLayerAlphaMask(m_layerId, m_oldAlphaMask);
        if (m_hasVectorMasks)
            m_doc->setLayerVectorMask(m_layerId, m_oldVectorMask);
        m_doc->setLayerTransform(m_layerId, m_oldTransform);
    }

private:
    LayerId m_oldAssetId;
    int m_oldWidth;
    int m_oldHeight;
    AffineTransform m_oldTransform;
    LayerId m_newAssetId;
    int m_newWidth;
    int m_newHeight;
    AffineTransform m_newTransform;
    QImage m_oldAlphaMask;
    QImage m_newAlphaMask;
    QImage m_oldVectorMask;
    QImage m_newVectorMask;
    bool m_hasAlphaMasks = false;
    bool m_hasVectorMasks = false;
};

} // namespace cc
