#pragma once

#include <QColor>
#include <QImage>
#include <QPolygonF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QUuid>

#include <memory>
#include <vector>

#include "../geometry/AffineTransform.h"
#include "BlendIf.h"
#include "Gradient.h"
#include "ImageEffects.h"
#include "ShapeEffects.h"
#include "TextEffects.h"

namespace cc {

class ProjectReader; // defined in serialization/ProjectFile.cpp

using LayerId = QUuid;

inline LayerId newLayerId() { return QUuid::createUuid(); }

enum class LayerType { Group, Image, Text, Shape, Background, Adjustment, Fill };
enum class BlendMode {
    Normal,
    Multiply,
    Screen,
    Overlay,
    Darken,
    Lighten,
    Add,
    ColorDodge,
    ColorBurn,
    HardLight,
    SoftLight,
    Difference,
    Exclusion
};
enum class TextAlignment { Left, Center, Right };
enum class ShapeKind { Rectangle, RoundedRect, Ellipse, Line, Polygon, ArrowRight,
                       ArrowCurved, Star, Badge, SpeechBubble, Heart,
                       Triangle, Diamond, Pentagon, Hexagon, Cross, Checkmark,
                       Cloud, Explosion };
enum class LayerColorTag { None, Red, Orange, Yellow, Green, Blue, Purple, Gray };

/// Granular per-pixel lock flags (Photoshop-style). A group lock applies to
/// all descendants via effective locks.
struct LayerLocks
{
    bool transparency = false; // lock transparent pixels
    bool pixels = false;       // lock image pixels
    bool position = false;     // lock position
    bool all = false;          // lock all (override)

    bool operator==(const LayerLocks& other) const noexcept
    {
        return transparency == other.transparency && pixels == other.pixels
            && position == other.position && all == other.all;
    }
};

/// Base class of every layer. UI/commands mutate layers ONLY through
/// Document methods; direct field access is the read path (renderer,
/// serializer) and interactive gestures (which push one command at release).
class Layer
{
public:
    explicit Layer(LayerType type);
    virtual ~Layer();

    Layer(const Layer&) = delete;
    Layer& operator=(const Layer&) = delete;

    LayerType type() const { return m_type; }
    const LayerId& id() const { return m_id; }

    QString name;
    bool visible = true;
    bool locked = false;        // legacy single lock flag (kept for compat)
    LayerLocks locks;           // granular per-pixel locks
    BlendMode blendMode = BlendMode::Normal;
    BlendIf blendIf;
    LayerColorTag colorTag = LayerColorTag::None;
    /// Camada transitória (prévia de mockup em diálogo modal): existe só
    /// em memória — NUNCA é serializada em save/autosave. Limpa a flag
    /// antes de promover a camada a definitiva.
    bool transient = false;

    AffineTransform transform;

    float opacity() const { return m_opacity; }
    void setOpacity(float value); // clamps to [0, 1]

    /// Fill opacity: applies to content only (not effects), like Photoshop.
    float fillOpacity() const { return m_fillOpacity; }
    void setFillOpacity(float value); // clamps to [0, 1]

    /// Clipped to layer below (clipping mask). When true, this layer only
    /// shows where the layer beneath is opaque.
    bool clipped = false;

    /// Layer link group: layers sharing the same link group move together.
    /// None = not linked.
    std::optional<qint64> linkGroup;

    /// Vector mask (applied together with alphaMask). Shape layers keep
    /// their outline in their own path instead.
    QImage vectorMask;

    /// Content bounds in LOCAL (untransformed) coordinates. Empty rect for
    /// layers without intrinsic geometry (Background).
    virtual QRectF contentBounds() const;

    virtual std::unique_ptr<Layer> deepCopy() const = 0;

protected:
    void copyCommonFrom(const Layer& other);

private:
    friend class ProjectReader;

    LayerType m_type;
    LayerId m_id;
    float m_opacity = 1.0f;
    float m_fillOpacity = 1.0f;
};

class GroupLayer final : public Layer
{
public:
    GroupLayer();
    std::unique_ptr<Layer> deepCopy() const override;
    QRectF contentBounds() const override;

    bool isExpanded = true;
    std::vector<std::unique_ptr<Layer>> children; // index 0 = bottom
};

class ImageLayer final : public Layer
{
public:
    ImageLayer();
    std::unique_ptr<Layer> deepCopy() const override;
    QRectF contentBounds() const override;

    LayerId assetId;
    int naturalWidth = 0;
    int naturalHeight = 0;
    // Máscara alfa opcional, não destrutiva. Branco mantém, preto oculta.
    QImage alphaMask;
    ImageEffects effects;

    // Cache transitória usada pelo renderizador durante gestos interativos.
    // Não é serializada: é reconstruída quando a imagem/máscara muda.
    mutable QImage maskedRenderCache;
    mutable qint64 maskedRenderSourceKey = 0;
    mutable qint64 maskedRenderMaskKey = 0;
};

class TextLayer final : public Layer
{
public:
    TextLayer();
    std::unique_ptr<Layer> deepCopy() const override;
    QRectF contentBounds() const override;

    QString content;
    QString fontFamily = QStringLiteral("Sans Serif");
    double sizePt = 48.0;
    bool bold = false;
    bool italic = false;
    bool underline = false;
    QColor color = QColor(17, 17, 17);
    double letterSpacingPx = 0.0;
    double lineHeightMult = 1.0;
    QSizeF box; // wrap box; empty = auto-size from font metrics
    TextAlignment align = TextAlignment::Center;
    TextEffects effects;
};

class ShapeLayer final : public Layer
{
public:
    ShapeLayer();
    std::unique_ptr<Layer> deepCopy() const override;
    QRectF contentBounds() const override;

    ShapeKind kind = ShapeKind::Rectangle;
    QColor fill = Qt::white;
    Gradient gradient; ///< preenchimento em gradiente (substitui fill quando ativo)
    QColor stroke = Qt::transparent;
    double strokeWidth = 0.0;
    double cornerRadius = 0.0;
    double strokeDashRatio = 0.0; ///< 0 = traço sólido; >0 = tracejado (escala do strokeWidth)
    QPolygonF points;
    // Estrela (kind == Star)
    int starPoints = 5;         ///< 3..16 pontas
    double starInnerRatio = 0.42; ///< 0.1..0.9 — afiação das pontas internas
    ShapeShadow shadow;
};

class BackgroundLayer final : public Layer
{
public:
    BackgroundLayer();
    std::unique_ptr<Layer> deepCopy() const override;

    QColor fill = Qt::white;
    Gradient gradient; ///< fundo em gradiente (substitui fill quando ativo)
};

/// Fill Layer (Photoshop-style): gera preenchimento procedural (cor/gradiente/padrão)
/// sem depender de um asset externo. Afetada pela opacity da camada.
enum class FillType { Solid, Gradient, Pattern };

class FillLayer final : public Layer
{
public:
    FillLayer();
    std::unique_ptr<Layer> deepCopy() const override;
    QRectF contentBounds() const override;

    FillType kind = FillType::Solid;
    QColor color = Qt::white;
    Gradient gradient;
    bool dither = false;
    bool alignWithLayer = false; // alinha gradiente/padrão ao frame da camada
    QPointF patternOffset;       // deslocamento do padrão em relação à camada
};

/// Adjustment Layer (Photoshop-style): aplica ajuste procedural a todas as
/// camadas abaixo (ou apenas às vinculadas por clipping mask).
enum class AdjustmentType { Levels, Curves, BrightnessContrast, HueSaturation,
                            ColorBalance, PhotoFilter, ChannelMixer,
                            Invert, Threshold, Posterize, GradientMap };

class AdjustmentLayer final : public Layer
{
public:
    AdjustmentLayer();
    std::unique_ptr<Layer> deepCopy() const override;
    QRectF contentBounds() const override;

    AdjustmentType kind = AdjustmentType::Levels;
    /// Parâmetros serializados como pares chave/valor (ex.: "brightness": 20).
    /// Mantém o formato simples e extensível sem depender de um schema fixo.
    std::vector<std::pair<QString, double>> parameters;
    bool useLegacy = false;
    bool preserveLuminosity = true;
};

std::unique_ptr<Layer> makeLayer(LayerType type);

} // namespace cc
