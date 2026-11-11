#include "AdjustmentEngine.h"

#include "core/image/ImageProcessing.h"

#include <QColor>
#include <QImage>
#include <QPainter>

#include <algorithm>
#include <cmath>

namespace cc {

double adjustmentParam(const AdjustmentParams& params, const QString& key, double def)
{
    for (const auto& [k, v] : params) {
        if (k == key)
            return v;
    }
    return def;
}

LevelsChannel levelsFromParams(const AdjustmentParams& params, const QString& prefix)
{
    LevelsChannel ch;
    ch.inBlack = static_cast<float>(adjustmentParam(params, prefix + QStringLiteral(".inBlack"), 0.0));
    ch.inWhite = static_cast<float>(adjustmentParam(params, prefix + QStringLiteral(".inWhite"), 1.0));
    ch.gamma = static_cast<float>(adjustmentParam(params, prefix + QStringLiteral(".gamma"), 1.0));
    ch.outBlack = static_cast<float>(adjustmentParam(params, prefix + QStringLiteral(".outBlack"), 0.0));
    ch.outWhite = static_cast<float>(adjustmentParam(params, prefix + QStringLiteral(".outWhite"), 1.0));
    return ch;
}

std::vector<CurvePoint> curveFromParams(const AdjustmentParams& params, const QString& prefix)
{
    const int count = static_cast<int>(adjustmentParam(params, prefix + QStringLiteral(".count"), 0.0));
    if (count < 2 || count > 64)
        return {};

    std::vector<CurvePoint> points;
    points.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        CurvePoint p;
        p.input = static_cast<float>(adjustmentParam(
            params, QStringLiteral("%1.%2.in").arg(prefix).arg(i), -1.0));
        p.output = static_cast<float>(adjustmentParam(
            params, QStringLiteral("%1.%2.out").arg(prefix).arg(i), -1.0));
        if (p.input < 0.0f || p.input > 1.0f || p.output < 0.0f || p.output > 1.0f)
            return {};
        points.push_back(p);
    }
    return points;
}

AdjustmentParams encodeLevels(const LevelsChannel& master, const LevelsChannel& red,
                              const LevelsChannel& green, const LevelsChannel& blue)
{
    AdjustmentParams out;
    const auto encode = [&out](const QString& prefix, const LevelsChannel& ch) {
        out.emplace_back(prefix + QStringLiteral(".inBlack"), ch.inBlack);
        out.emplace_back(prefix + QStringLiteral(".inWhite"), ch.inWhite);
        out.emplace_back(prefix + QStringLiteral(".gamma"), ch.gamma);
        out.emplace_back(prefix + QStringLiteral(".outBlack"), ch.outBlack);
        out.emplace_back(prefix + QStringLiteral(".outWhite"), ch.outWhite);
    };
    encode(QStringLiteral("levels.master"), master);
    encode(QStringLiteral("levels.r"), red);
    encode(QStringLiteral("levels.g"), green);
    encode(QStringLiteral("levels.b"), blue);
    return out;
}

AdjustmentParams encodeCurves(const std::vector<CurvePoint>& master,
                              const std::vector<CurvePoint>& red,
                              const std::vector<CurvePoint>& green,
                              const std::vector<CurvePoint>& blue)
{
    AdjustmentParams out;
    const auto encode = [&out](const QString& prefix, const std::vector<CurvePoint>& pts) {
        out.emplace_back(prefix + QStringLiteral(".count"), static_cast<double>(pts.size()));
        for (size_t i = 0; i < pts.size(); ++i) {
            out.emplace_back(QStringLiteral("%1.%2.in").arg(prefix).arg(static_cast<qulonglong>(i)),
                             pts[i].input);
            out.emplace_back(QStringLiteral("%1.%2.out").arg(prefix).arg(static_cast<qulonglong>(i)),
                             pts[i].output);
        }
    };
    encode(QStringLiteral("curves.master"), master);
    encode(QStringLiteral("curves.r"), red);
    encode(QStringLiteral("curves.g"), green);
    encode(QStringLiteral("curves.b"), blue);
    return out;
}

QImage applyAdjustmentParameters(const QImage& source, AdjustmentType kind,
                                 const AdjustmentParams& params, float strength)
{
    if (source.isNull())
        return source;

    QImage adjusted;
    switch (kind) {
    case AdjustmentType::Levels:
        adjusted = ImageProcessing::applyLevels(
            source, levelsFromParams(params, QStringLiteral("levels.master")),
            levelsFromParams(params, QStringLiteral("levels.r")),
            levelsFromParams(params, QStringLiteral("levels.g")),
            levelsFromParams(params, QStringLiteral("levels.b")));
        break;

    case AdjustmentType::Curves:
        adjusted = ImageProcessing::applyCurves(
            source, curveFromParams(params, QStringLiteral("curves.master")),
            curveFromParams(params, QStringLiteral("curves.r")),
            curveFromParams(params, QStringLiteral("curves.g")),
            curveFromParams(params, QStringLiteral("curves.b")));
        break;

    case AdjustmentType::ChannelMixer:
        adjusted = ImageProcessing::applyChannelMixer(
            source, adjustmentParam(params, QStringLiteral("redGain"), 1.0),
            adjustmentParam(params, QStringLiteral("greenGain"), 1.0),
            adjustmentParam(params, QStringLiteral("blueGain"), 1.0));
        break;

    case AdjustmentType::BrightnessContrast:
        adjusted = ImageProcessing::adjustColors(
            source, adjustmentParam(params, QStringLiteral("brightness"), 10.0),
            adjustmentParam(params, QStringLiteral("contrast"), 10.0), 0, 0);
        break;

    case AdjustmentType::HueSaturation:
        adjusted = ImageProcessing::adjustColors(
            source, 0, 0, adjustmentParam(params, QStringLiteral("saturation"), 0.0), 0,
            adjustmentParam(params, QStringLiteral("hue"), 0.0));
        break;

    case AdjustmentType::ColorBalance:
    case AdjustmentType::PhotoFilter:
        adjusted = ImageProcessing::adjustColors(
            source, 0, 0, 0, adjustmentParam(params, QStringLiteral("temperature"), 20.0));
        break;

    case AdjustmentType::Invert:
    case AdjustmentType::Posterize:
    case AdjustmentType::Threshold:
    {
        using PF = ImageProcessing::PresetFilter;
        const PF filter = kind == AdjustmentType::Invert       ? PF::Invert
                          : kind == AdjustmentType::Posterize  ? PF::Posterize
                                                               : PF::Threshold;
        adjusted = ImageProcessing::applyPresetFilter(source, filter);
        break;
    }

    case AdjustmentType::GradientMap:
        adjusted = ImageProcessing::applyGradientMap(
            source,
            QColor::fromRgbF(adjustmentParam(params, QStringLiteral("color1.r"), 0.0),
                             adjustmentParam(params, QStringLiteral("color1.g"), 0.0),
                             adjustmentParam(params, QStringLiteral("color1.b"), 0.0)),
            QColor::fromRgbF(adjustmentParam(params, QStringLiteral("color2.r"), 1.0),
                             adjustmentParam(params, QStringLiteral("color2.g"), 1.0),
                             adjustmentParam(params, QStringLiteral("color2.b"), 1.0)),
            adjustmentParam(params, QStringLiteral("reverse"), 0.0) > 0.5);
        break;
    }

    // Identidade detectada pelos próprios motores (curva vazia, níveis
    // identidade, ganhos 1): devolve a imagem original sem custo.
    if (adjusted.isNull()
        || (adjusted.size() == source.size()
            && adjusted.format() == source.format()
            && adjusted == source))
        return source;

    if (adjusted.format() != source.format())
        adjusted = adjusted.convertToFormat(source.format());

    const float s = std::clamp(strength, 0.0f, 1.0f);
    if (s >= 0.999f)
        return adjusted;

    // Mistura original→ajustado (equivalente à opacidade da camada).
    QImage blended(source.size(), source.format());
    blended.fill(Qt::transparent);
    QPainter bp(&blended);
    bp.drawImage(0, 0, source);
    bp.setOpacity(s);
    bp.drawImage(0, 0, adjusted);
    bp.end();
    return blended;
}

} // namespace cc
