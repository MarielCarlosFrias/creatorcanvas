#include "SelectionTransform.h"

#include "core/Document.h"
#include "core/commands/SelectionTransformCommands.h"
#include "core/history/CommandStack.h"
#include "core/selection/SelectionEngine.h"

#include <QRectF>
#include <QTransform>

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

namespace cc {
namespace {

constexpr double kParameterEpsilon = 1e-12;
constexpr double kCoordinateSnapEpsilon = 1e-9;

bool finitePoint(const QPointF& point)
{
    return std::isfinite(point.x()) && std::isfinite(point.y());
}

QRect exactBounds(const QImage& selection)
{
    if (selection.isNull())
        return {};

    int left = selection.width();
    int top = selection.height();
    int right = -1;
    int bottom = -1;
    for (int y = 0; y < selection.height(); ++y) {
        const uchar* row = selection.constScanLine(y);
        for (int x = 0; x < selection.width(); ++x) {
            if (row[x] == 0)
                continue;
            left = std::min(left, x);
            top = std::min(top, y);
            right = std::max(right, x);
            bottom = std::max(bottom, y);
        }
    }
    if (right < 0)
        return {};
    return QRect(left, top, right - left + 1, bottom - top + 1);
}

uchar coverageAt(const QImage& source, int x, int y)
{
    if (x < 0 || y < 0 || x >= source.width() || y >= source.height())
        return 0;
    return source.constScanLine(y)[x];
}

double snapCoordinate(double value)
{
    const double nearest = std::round(value);
    return std::abs(value - nearest) < kCoordinateSnapEpsilon ? nearest : value;
}

bool nearlyEqual(double a, double b)
{
    return std::abs(a - b) <= kParameterEpsilon;
}

} // namespace

bool SelectionTransform::isValid(const SelectionTransformParameters& parameters)
{
    return finitePoint(parameters.translation)
        && std::isfinite(parameters.scaleXPercent)
        && std::isfinite(parameters.scaleYPercent)
        && std::isfinite(parameters.rotationDegrees)
        && std::abs(parameters.scaleXPercent) > kParameterEpsilon
        && std::abs(parameters.scaleYPercent) > kParameterEpsilon
        && (!parameters.pivot || finitePoint(*parameters.pivot));
}

bool SelectionTransform::isIdentity(const SelectionTransformParameters& parameters)
{
    if (!isValid(parameters))
        return false;
    const double angle = std::fmod(parameters.rotationDegrees, 360.0);
    return nearlyEqual(parameters.translation.x(), 0.0)
        && nearlyEqual(parameters.translation.y(), 0.0)
        && nearlyEqual(parameters.scaleXPercent, 100.0)
        && nearlyEqual(parameters.scaleYPercent, 100.0)
        && nearlyEqual(angle, 0.0);
}

QPointF SelectionTransform::defaultPivot(const QImage& selection)
{
    if (selection.isNull())
        return {};
    const QImage source = SelectionEngine::normalize(selection);
    const QRect bounds = exactBounds(source);
    return QRectF(bounds.isEmpty() ? QRect(QPoint(0, 0), source.size()) : bounds)
        .center();
}

QImage SelectionTransform::apply(const QImage& selection,
                                 const SelectionTransformParameters& parameters)
{
    if (selection.isNull() || !isValid(parameters))
        return {};

    const QImage source = SelectionEngine::normalize(selection);
    if (source.isNull() || source.width() <= 0 || source.height() <= 0)
        return {};
    if (isIdentity(parameters))
        return source;

    const QRect bounds = exactBounds(source);
    if (bounds.isEmpty())
        return {};

    const QPointF pivot = parameters.pivot.value_or(QRectF(bounds).center());
    QTransform oldToNew;
    oldToNew.translate(pivot.x() + parameters.translation.x(),
                       pivot.y() + parameters.translation.y());
    oldToNew.rotate(parameters.rotationDegrees);
    oldToNew.scale(parameters.scaleXPercent / 100.0,
                   parameters.scaleYPercent / 100.0);
    oldToNew.translate(-pivot.x(), -pivot.y());

    bool invertible = false;
    const QTransform newToOld = oldToNew.inverted(&invertible);
    if (!invertible)
        return {};

    QImage output(source.size(), QImage::Format_Alpha8);
    if (output.isNull())
        return {};
    output.fill(0);
    bool anyCoverage = false;

    // A máscara representa cobertura nos centros dos pixels. A amostragem
    // inversa evita buracos ao ampliar e zero fora da fonte recorta o contorno
    // sem mudar de Document/Layer nem das dimensões desse espaço.
    for (int y = 0; y < output.height(); ++y) {
        uchar* destination = output.scanLine(y);
        for (int x = 0; x < output.width(); ++x) {
            const QPointF sourceCenter = newToOld.map(QPointF(x + 0.5, y + 0.5));
            if (!finitePoint(sourceCenter)
                || sourceCenter.x() < -0.5
                || sourceCenter.y() < -0.5
                || sourceCenter.x() > source.width() + 0.5
                || sourceCenter.y() > source.height() + 0.5) {
                // Além de não contribuir cobertura, coordenadas extremas não
                // podem ser convertidas com segurança para int (overflow).
                continue;
            }

            const double sx = snapCoordinate(sourceCenter.x()) - 0.5;
            const double sy = snapCoordinate(sourceCenter.y()) - 0.5;
            const int x0 = static_cast<int>(std::floor(sx));
            const int y0 = static_cast<int>(std::floor(sy));
            const double fx = sx - x0;
            const double fy = sy - y0;

            const double top = coverageAt(source, x0, y0) * (1.0 - fx)
                + coverageAt(source, x0 + 1, y0) * fx;
            const double bottom = coverageAt(source, x0, y0 + 1) * (1.0 - fx)
                + coverageAt(source, x0 + 1, y0 + 1) * fx;
            const double value = top * (1.0 - fy) + bottom * fy;
            const int coverage = std::clamp(static_cast<int>(std::lround(value)), 0, 255);
            destination[x] = static_cast<uchar>(coverage);
            anyCoverage = anyCoverage || coverage != 0;
        }
    }

    return anyCoverage ? output : QImage();
}

bool SelectionTransform::apply(Document& document, CommandStack* history,
                               const SelectionTransformParameters& parameters,
                               const QString& commandName)
{
    const QImage oldSelection = document.pixelSelection();
    if (oldSelection.isNull() || !isValid(parameters))
        return false;

    const SelectionSpace space = document.pixelSelectionSpace();
    QImage transformed = apply(oldSelection, parameters);
    if (oldSelection == transformed)
        return true;

    if (history) {
        history->execute(std::make_unique<TransformSelectionCommand>(
            document, oldSelection, space, std::move(transformed), space, commandName));
    } else if (transformed.isNull()) {
        document.clearPixelSelection();
    } else {
        document.setPixelSelection(std::move(transformed), space);
    }
    return true;
}

} // namespace cc
