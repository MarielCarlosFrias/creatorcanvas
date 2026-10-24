#pragma once

#include "BaseCommands.h"
#include "core/layers/TextEffects.h"
#include "core/layers/ImageEffects.h"

#include <type_traits>

namespace cc {

/// Generic property change routed through a Document setter, so clamping and
/// change notifications behave exactly like a direct edit.
///
/// CONVENTION: Document setters used with this command MUST take the value
/// BY VALUE (ValueType), e.g. setLayerName(id, QString). A setter taking
/// const T& will fail to compile here — that is intentional.
template <typename T>
class LayerPropertyCommand final : public LayerCommand
{
public:
    using ValueType = std::decay_t<T>;
    using ApplyFn = bool (Document::*)(const LayerId&, ValueType);

    LayerPropertyCommand(Document& doc, const LayerId& id, QString name,
                         ApplyFn apply, ValueType oldValue, ValueType newValue)
        : LayerCommand(name, doc, id, determineLockReq(name))
        , m_apply(apply)
        , m_oldValue(std::move(oldValue))
        , m_newValue(std::move(newValue))
    {
    }

    static LockRequirement determineLockReq(const QString& name)
    {
        if (name.startsWith(QStringLiteral("layer.lock")))
            return LockRequirement::None;
        if (name.startsWith(QStringLiteral("text.")) || name.startsWith(QStringLiteral("shape.")) ||
            name == QStringLiteral("layer.text"))
            return LockRequirement::Pixels;
        return LockRequirement::All;
    }

    void redo() override
    {
        if (!canExecute())
            return;
        (m_doc->*m_apply)(m_layerId, m_newValue);
    }

    void undo() override
    {
        if (!canExecute())
            return;
        (m_doc->*m_apply)(m_layerId, m_oldValue);
    }

    bool mergeWith(const Command& other) override
    {
        const auto* rhs = dynamic_cast<const LayerPropertyCommand<T>*>(&other);
        const bool mergeableTextEdit = name().startsWith(QStringLiteral("text.")) ||
                                       name() == QStringLiteral("layer.text");
        if (!mergeableTextEdit || !rhs || rhs->m_doc != m_doc || rhs->m_layerId != m_layerId ||
            rhs->m_apply != m_apply || rhs->name() != name())
            return false;
        m_newValue = rhs->m_newValue;
        return true;
    }

private:
    ApplyFn m_apply;
    ValueType m_oldValue;
    ValueType m_newValue;
};

using RenameLayerCommand     = LayerPropertyCommand<QString>;
using LayerVisibilityCommand = LayerPropertyCommand<bool>;
using LayerLockCommand       = LayerPropertyCommand<bool>;
using LayerColorTagCommand   = LayerPropertyCommand<LayerColorTag>;
using LayerOpacityCommand    = LayerPropertyCommand<float>;
using LayerBlendModeCommand  = LayerPropertyCommand<BlendMode>;
using LayerBlendIfCommand    = LayerPropertyCommand<BlendIf>;
using LayerFillOpacityCommand = LayerPropertyCommand<float>;
using LayerClippedCommand    = LayerPropertyCommand<bool>;
using LayerLinkGroupCommand  = LayerPropertyCommand<std::optional<qint64>>;
using LayerVectorMaskCommand = LayerPropertyCommand<QImage>;
using LayerLocksCommand      = LayerPropertyCommand<LayerLocks>;

// Comandos de histórico fortemente tipados para operações de formas geométricas (ShapeLayer)
using SetShapeFillCommand         = LayerPropertyCommand<QColor>;
using SetShapeStrokeCommand       = LayerPropertyCommand<QColor>;
using SetShapeStrokeWidthCommand  = LayerPropertyCommand<double>;
using SetShapeCornerRadiusCommand = LayerPropertyCommand<double>;
using SetShapeKindCommand         = LayerPropertyCommand<ShapeKind>;
using SetShapeGradientCommand     = LayerPropertyCommand<Gradient>;
using SetShapeStarPointsCommand   = LayerPropertyCommand<int>;
using SetShapeStarInnerRatioCommand = LayerPropertyCommand<double>;
using SetShapeStrokeDashRatioCommand = LayerPropertyCommand<double>;
using SetShapeShadowCommand       = LayerPropertyCommand<ShapeShadow>;
using SetBackgroundGradientCommand = LayerPropertyCommand<Gradient>;

// Comandos de histórico para operações de texto (TextLayer)
using SetTextFontFamilyCommand = LayerPropertyCommand<QString>;
using SetTextSizeCommand       = LayerPropertyCommand<double>;
using SetTextBoldCommand       = LayerPropertyCommand<bool>;
using SetTextItalicCommand     = LayerPropertyCommand<bool>;
using SetTextUnderlineCommand  = LayerPropertyCommand<bool>;
using SetTextColorCommand      = LayerPropertyCommand<QColor>;
using SetTextAlignCommand      = LayerPropertyCommand<TextAlignment>;
using SetTextLetterSpacingCommand = LayerPropertyCommand<double>;
using SetTextLineHeightCommand   = LayerPropertyCommand<double>;

/// Applies a full text-effects set (outline + shadow).
class SetLayerTextEffectsCommand final : public LayerCommand
{
public:
    SetLayerTextEffectsCommand(Document& doc, const LayerId& id,
                               TextEffects oldValue, TextEffects newValue)
        : LayerCommand(QStringLiteral("layer.textEffects"), doc, id, LockRequirement::Pixels)
        , m_old(std::move(oldValue))
        , m_new(std::move(newValue))
    {
    }

    void redo() override
    {
        if (!canExecute())
            return;
        m_doc->setLayerTextEffects(m_layerId, m_new);
    }

    void undo() override
    {
        if (!canExecute())
            return;
        m_doc->setLayerTextEffects(m_layerId, m_old);
    }

    bool mergeWith(const Command& other) override
    {
        const auto* rhs = dynamic_cast<const SetLayerTextEffectsCommand*>(&other);
        if (!rhs || rhs->m_doc != m_doc || rhs->m_layerId != m_layerId)
            return false;
        m_new = rhs->m_new;
        return true;
    }

private:
    TextEffects m_old;
    TextEffects m_new;
};

/// Applies image effects (outline/glow/sticker).
class SetLayerImageEffectsCommand final : public LayerCommand
{
public:
    SetLayerImageEffectsCommand(Document& doc, const LayerId& id,
                                ImageEffects oldValue, ImageEffects newValue)
        : LayerCommand(QStringLiteral("layer.imageEffects"), doc, id, LockRequirement::Pixels)
        , m_old(std::move(oldValue))
        , m_new(std::move(newValue))
    {
    }

    void redo() override
    {
        if (!canExecute())
            return;
        m_doc->setLayerImageEffects(m_layerId, m_new);
    }

    void undo() override
    {
        if (!canExecute())
            return;
        m_doc->setLayerImageEffects(m_layerId, m_old);
    }

    bool mergeWith(const Command& other) override
    {
        const auto* next = dynamic_cast<const SetLayerImageEffectsCommand*>(&other);
        if (!next || next->m_doc != m_doc || next->m_layerId != m_layerId)
            return false;
        m_new = next->m_new;
        return true;
    }

private:
    ImageEffects m_old;
    ImageEffects m_new;
};

/// Troca o asset bitmap de uma camada de imagem (filtros destrutivos como
/// Inverter/Posterizar/Threshold). O asset antigo continua no AssetStore,
/// então undo/redo é só restaurar o id e as dimensões gravadas.
class SetLayerImageAssetCommand final : public LayerCommand
{
public:
    SetLayerImageAssetCommand(Document& doc, const LayerId& layerId,
                              LayerId oldAssetId, int oldW, int oldH,
                              LayerId newAssetId, int newW, int newH)
        : LayerCommand(QStringLiteral("layer.imageAsset"), doc, layerId, LockRequirement::Pixels)
        , m_oldAssetId(std::move(oldAssetId))
        , m_oldW(oldW)
        , m_oldH(oldH)
        , m_newAssetId(std::move(newAssetId))
        , m_newW(newW)
        , m_newH(newH)
    {
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
        m_doc->setImageLayerAsset(m_layerId, m_newAssetId, m_newW, m_newH);
    }

    void undo() override
    {
        if (!canExecute())
            return;
        m_doc->setImageLayerAsset(m_layerId, m_oldAssetId, m_oldW, m_oldH);
    }

private:
    LayerId m_oldAssetId;
    int m_oldW = 0;
    int m_oldH = 0;
    LayerId m_newAssetId;
    int m_newW = 0;
    int m_newH = 0;
};

/// Resizes a text layer's wrap box (handle gesture on text).
class SetLayerTextBoxCommand final : public LayerCommand
{
public:
    SetLayerTextBoxCommand(Document& doc, const LayerId& id,
                           QSizeF oldValue, QSizeF newValue)
        : LayerCommand(QStringLiteral("layer.textBox"), doc, id, LockRequirement::Position)
        , m_old(std::move(oldValue))
        , m_new(std::move(newValue))
    {
    }

    void redo() override
    {
        if (!canExecute())
            return;
        m_doc->setLayerTextBox(m_layerId, m_new);
    }

    void undo() override
    {
        if (!canExecute())
            return;
        m_doc->setLayerTextBox(m_layerId, m_old);
    }

private:
    QSizeF m_old;
    QSizeF m_new;
};

} // namespace cc
