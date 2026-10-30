#include "ImageProcessing.h"

#include <QLinearGradient>
#include <QLineF>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace cc {

QImage ImageProcessing::cropImage(const QImage& source, const QRect& cropRect)
{
    if (source.isNull())
        return {};

    // Intersecta a regiao solicitada com os limites reais da imagem
    const QRect validRect = cropRect.intersected(source.rect());
    if (validRect.isEmpty() || !validRect.isValid())
        return {};

    return source.copy(validRect);
}

QRect ImageProcessing::calculateAspectCropRect(const QSize& sourceSize, double aspectRatio)
{
    if (sourceSize.isEmpty() || aspectRatio <= 0.0)
        return QRect(QPoint(0, 0), sourceSize);

    const double srcAspect = static_cast<double>(sourceSize.width()) / sourceSize.height();
    int cropW = sourceSize.width();
    int cropH = sourceSize.height();

    // Ajusta as dimensoes preservando a proporcao desejada centralizada
    if (srcAspect > aspectRatio) {
        cropW = std::clamp(static_cast<int>(std::round(cropH * aspectRatio)), 1, sourceSize.width());
    } else {
        cropH = std::clamp(static_cast<int>(std::round(cropW / aspectRatio)), 1, sourceSize.height());
    }

    const int x = (sourceSize.width() - cropW) / 2;
    const int y = (sourceSize.height() - cropH) / 2;
    return QRect(x, y, cropW, cropH);
}

QImage ImageProcessing::removeBackground(const QImage& source, const QPoint& seedPoint,
                                         int tolerance, bool contiguous)
{
    if (source.isNull() || !source.rect().contains(seedPoint))
        return source;

    // Converte para Format_ARGB32 para acesso direto aos canais sem premultiplicacao
    QImage result = source.convertToFormat(QImage::Format_ARGB32);
    const int w = result.width();
    const int h = result.height();

    const QRgb targetRgb = result.pixel(seedPoint);
    const int tr = qRed(targetRgb);
    const int tg = qGreen(targetRgb);
    const int tb = qBlue(targetRgb);
    const int ta = qAlpha(targetRgb);

    // Mapeia tolerancia 0..100 para limiar de distancia euclidiana RGB
    // Distancia maxima em RGB e sqrt(255^2 * 3) ~= 441.67
    const double maxDist = (ta == 0) ? 255.0 : 441.67295593;
    const double threshold = (std::clamp(tolerance, 0, 100) / 100.0) * maxDist;
    const double thresholdSq = threshold * threshold;

    auto matches = [&](QRgb pixel) -> bool {
        const int pa = qAlpha(pixel);
        if (ta == 0 && pa == 0)
            return true;
        const int pr = qRed(pixel);
        const int pg = qGreen(pixel);
        const int pb = qBlue(pixel);
        const double dr = pr - tr;
        const double dg = pg - tg;
        const double db = pb - tb;
        const double distSq = dr * dr + dg * dg + db * db;
        return distSq <= thresholdSq;
    };

    if (!contiguous) {
        // Modo nao-contiguo: remove todas as cores correspondentes na imagem inteira
        for (int y = 0; y < h; ++y) {
            QRgb* scanline = reinterpret_cast<QRgb*>(result.scanLine(y));
            for (int x = 0; x < w; ++x) {
                if (matches(scanline[x])) {
                    scanline[x] = 0x00000000;
                }
            }
        }
        return result;
    }

    // Modo contiguo: flood-fill baseado em pilha (DFS) com pop_back para consumo minimo de memoria
    std::vector<uint8_t> visited(w * h, 0);
    std::vector<QPoint> stack;
    stack.reserve(std::min(w * h, 65536));

    stack.push_back(seedPoint);
    visited[seedPoint.y() * w + seedPoint.x()] = 1;

    while (!stack.empty()) {
        const QPoint pt = stack.back();
        stack.pop_back();
        const int px = pt.x();
        const int py = pt.y();

        QRgb* scanline = reinterpret_cast<QRgb*>(result.scanLine(py));
        if (!matches(scanline[px]))
            continue;

        // Limpa o pixel para transparencia total
        scanline[px] = 0x00000000;

        // 4 vizinhos conectados (cima, baixo, esquerda, direita)
        static const int dx[4] = { 1, -1, 0, 0 };
        static const int dy[4] = { 0, 0, 1, -1 };

        for (int i = 0; i < 4; ++i) {
            const int nx = px + dx[i];
            const int ny = py + dy[i];

            if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                const int idx = ny * w + nx;
                if (!visited[idx]) {
                    visited[idx] = 1;
                    const QRgb neighborPixel = reinterpret_cast<const QRgb*>(result.constScanLine(ny))[nx];
                    if (matches(neighborPixel)) {
                        stack.push_back(QPoint(nx, ny));
                    }
                }
            }
        }
    }

    return result;
}

ScissorsCutResult ImageProcessing::scissorsCut(const QImage& source, const QPolygonF& polygon,
                                               bool keepInside, bool cropToBoundingRect)
{
    if (source.isNull() || polygon.size() < 3)
        return { source, QPoint(0, 0) };

    if (keepInside) {
        // Mantem o conteudo dentro do poligono e descarta o exterior
        QImage result(source.size(), QImage::Format_ARGB32_Premultiplied);
        result.fill(Qt::transparent);

        {
            QPainter painter(&result);
            painter.setRenderHint(QPainter::Antialiasing, true);
            QPainterPath path;
            path.addPolygon(polygon);
            painter.setClipPath(path);
            painter.drawImage(0, 0, source);
        }

        if (cropToBoundingRect) {
            const QRect bounding = polygon.boundingRect().toAlignedRect().intersected(source.rect());
            if (bounding.isValid() && !bounding.isEmpty()) {
                return { result.copy(bounding), bounding.topLeft() };
            }
        }
        return { result, QPoint(0, 0) };
    } else {
        // Remove o conteudo dentro do poligono (cria recorte/furo transparente)
        QImage result = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        {
            QPainter painter(&result);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setCompositionMode(QPainter::CompositionMode_Clear);
            QPainterPath path;
            path.addPolygon(polygon);
            painter.fillPath(path, Qt::transparent);
        }
        return { result, QPoint(0, 0) };
    }
}

QImage ImageProcessing::cloneStamp(const QImage& target, const QImage& source,
                                   const QPoint& srcPoint, const QPoint& dstPoint,
                                   int radius, qreal opacity, qreal hardness)
{
    if (target.isNull() || source.isNull() || radius <= 0)
        return target;

    QImage result = target.convertToFormat(QImage::Format_ARGB32);
    QImage srcConv = source.convertToFormat(QImage::Format_ARGB32);

    const int clampedRadius = std::max(1, radius);
    const qreal clampedOpacity = std::clamp(opacity, 0.0, 1.0);
    const qreal clampedHardness = std::clamp(hardness, 0.0, 1.0);
    const qreal rInner = clampedRadius * clampedHardness;
    const qreal rOuter = clampedRadius;

    const int minX = std::max(0, dstPoint.x() - clampedRadius);
    const int maxX = std::min(result.width() - 1, dstPoint.x() + clampedRadius);
    const int minY = std::max(0, dstPoint.y() - clampedRadius);
    const int maxY = std::min(result.height() - 1, dstPoint.y() + clampedRadius);

    const int offsetX = srcPoint.x() - dstPoint.x();
    const int offsetY = srcPoint.y() - dstPoint.y();

    for (int dy = minY; dy <= maxY; ++dy) {
        const int sy = dy + offsetY;
        if (sy < 0 || sy >= srcConv.height())
            continue;

        QRgb* dstLine = reinterpret_cast<QRgb*>(result.scanLine(dy));
        const QRgb* srcLine = reinterpret_cast<const QRgb*>(srcConv.constScanLine(sy));

        for (int dx = minX; dx <= maxX; ++dx) {
            const int sx = dx + offsetX;
            if (sx < 0 || sx >= srcConv.width())
                continue;

            const double dist = std::hypot(dx - dstPoint.x(), dy - dstPoint.y());
            if (dist > rOuter)
                continue;

            // Fator de suavizacao da borda (radial falloff)
            double weight = 1.0;
            if (dist > rInner && rOuter > rInner) {
                weight = 1.0 - (dist - rInner) / (rOuter - rInner);
            }
            const double alpha = weight * clampedOpacity;
            if (alpha <= 0.0)
                continue;

            const QRgb sPixel = srcLine[sx];
            const QRgb dPixel = dstLine[dx];

            const int sa = qAlpha(sPixel);
            const int sr = qRed(sPixel);
            const int sg = qGreen(sPixel);
            const int sb = qBlue(sPixel);

            const int da = qAlpha(dPixel);
            const int dr = qRed(dPixel);
            const int dg = qGreen(dPixel);
            const int db = qBlue(dPixel);

            // Mistura alfa ponderada entre origem e destino
            const double effectiveAlpha = alpha * (sa / 255.0);
            const int outR = qRound(sr * effectiveAlpha + dr * (1.0 - effectiveAlpha));
            const int outG = qRound(sg * effectiveAlpha + dg * (1.0 - effectiveAlpha));
            const int outB = qRound(sb * effectiveAlpha + db * (1.0 - effectiveAlpha));
            const int outA = qRound(sa * effectiveAlpha + da * (1.0 - effectiveAlpha));

            dstLine[dx] = qRgba(std::clamp(outR, 0, 255),
                                std::clamp(outG, 0, 255),
                                std::clamp(outB, 0, 255),
                                std::clamp(outA, 0, 255));
        }
    }

    return result;
}

QImage ImageProcessing::paintStroke(const QImage& target, const QPointF& prevPoint, const QPointF& curPoint,
                                   BrushType brush, const QColor& color, int size, qreal opacity)
{
    if (target.isNull())
        return target;

    QImage result = target;
    if (result.format() != QImage::Format_ARGB32_Premultiplied && result.format() != QImage::Format_ARGB32)
        result = result.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    QPainter painter(&result);

    const int clampedSize = std::clamp(size, 1, 300);
    const qreal clampedOpacity = std::clamp(opacity, 0.0, 1.0);

    switch (brush) {
    case BrushType::Eraser: {
        painter.setCompositionMode(QPainter::CompositionMode_Clear);
        painter.setRenderHint(QPainter::Antialiasing, true);
        if (prevPoint == curPoint) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(Qt::transparent);
            painter.drawEllipse(curPoint, clampedSize / 2.0, clampedSize / 2.0);
        } else {
            QPen pen(Qt::transparent, clampedSize, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            painter.setPen(pen);
            painter.drawLine(prevPoint, curPoint);
        }
        break;
    }
    case BrushType::Pencil: {
        painter.setRenderHint(QPainter::Antialiasing, false);
        QColor c = color;
        c.setAlphaF(clampedOpacity);
        if (prevPoint == curPoint) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(c);
            painter.drawRect(QRectF(curPoint.x() - clampedSize / 2.0, curPoint.y() - clampedSize / 2.0, clampedSize, clampedSize));
        } else {
            QPen pen(c, clampedSize, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin);
            painter.setPen(pen);
            painter.drawLine(prevPoint, curPoint);
        }
        break;
    }
    case BrushType::Highlighter: {
        painter.setRenderHint(QPainter::Antialiasing, true);
        QColor c = color;
        c.setAlphaF(std::min(0.4, clampedOpacity * 0.4));
        QPen pen(c, clampedSize * 1.5, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin);
        painter.setPen(pen);
        painter.drawLine(prevPoint, curPoint);
        break;
    }
    case BrushType::Airbrush: {
        painter.setRenderHint(QPainter::Antialiasing, true);
        const double dist = QLineF(prevPoint, curPoint).length();
        const int steps = std::max(1, static_cast<int>(dist / 2.0));
        QColor c = color;
        c.setAlphaF(std::min(1.0, clampedOpacity * 0.15));
        painter.setPen(Qt::NoPen);
        painter.setBrush(c);

        for (int s = 0; s <= steps; ++s) {
            const double t = steps > 0 ? (static_cast<double>(s) / steps) : 0.0;
            const QPointF pt = prevPoint + (curPoint - prevPoint) * t;
            const int drops = std::clamp(clampedSize * 2, 8, 80);
            for (int d = 0; d < drops; ++d) {
                const double angle = (static_cast<double>(rand()) / RAND_MAX) * 2.0 * M_PI;
                const double r = std::sqrt(static_cast<double>(rand()) / RAND_MAX) * (clampedSize / 2.0);
                const QPointF dropPt = pt + QPointF(std::cos(angle) * r, std::sin(angle) * r);
                painter.drawEllipse(dropPt, 1.0, 1.0);
            }
        }
        break;
    }
    case BrushType::Brush:
    default: {
        painter.setRenderHint(QPainter::Antialiasing, true);
        QColor c = color;
        c.setAlphaF(clampedOpacity);
        if (prevPoint == curPoint) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(c);
            painter.drawEllipse(curPoint, clampedSize / 2.0, clampedSize / 2.0);
        } else {
            QPen pen(c, clampedSize, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            painter.setPen(pen);
            painter.drawLine(prevPoint, curPoint);
        }
        break;
    }
    }

    return result;
}

QImage ImageProcessing::floodFill(const QImage& source, const QPoint& seedPoint,
                                  const QColor& fillColor, int tolerance)
{
    if (source.isNull() || !source.rect().contains(seedPoint))
        return source;

    QImage result = source.convertToFormat(QImage::Format_ARGB32);
    const int w = result.width();
    const int h = result.height();

    const QRgb targetRgb = result.pixel(seedPoint);
    const QRgb replacementRgb = fillColor.rgba();
    if (targetRgb == replacementRgb)
        return result;

    const int tr = qRed(targetRgb);
    const int tg = qGreen(targetRgb);
    const int tb = qBlue(targetRgb);
    const int ta = qAlpha(targetRgb);

    const double maxDist = std::sqrt(255.0 * 255.0 * 4.0);
    const double tolDistance = (tolerance / 100.0) * maxDist;
    const double tolDistanceSq = tolDistance * tolDistance;

    std::vector<bool> visited(w * h, false);
    std::vector<QPoint> queue;
    queue.reserve(w * 8);
    queue.push_back(seedPoint);
    visited[seedPoint.y() * w + seedPoint.x()] = true;

    auto colorMatch = [&](QRgb px) -> bool {
        const double dr = qRed(px) - tr;
        const double dg = qGreen(px) - tg;
        const double db = qBlue(px) - tb;
        const double da = qAlpha(px) - ta;
        const double distSq = dr * dr + dg * dg + db * db + da * da;
        return distSq <= tolDistanceSq;
    };

    size_t head = 0;
    while (head < queue.size()) {
        const QPoint p = queue[head++];
        reinterpret_cast<QRgb*>(result.scanLine(p.y()))[p.x()] = replacementRgb;

        const QPoint neighbors[4] = {
            QPoint(p.x() + 1, p.y()),
            QPoint(p.x() - 1, p.y()),
            QPoint(p.x(), p.y() + 1),
            QPoint(p.x(), p.y() - 1)
        };

        for (const QPoint& n : neighbors) {
            if (n.x() >= 0 && n.x() < w && n.y() >= 0 && n.y() < h) {
                const int idx = n.y() * w + n.x();
                if (!visited[idx]) {
                    visited[idx] = true;
                    const QRgb neighborColor = reinterpret_cast<const QRgb*>(result.constScanLine(n.y()))[n.x()];
                    if (colorMatch(neighborColor)) {
                        queue.push_back(n);
                    }
                }
            }
        }
    }

    return result;
}

QImage ImageProcessing::adjustColors(const QImage& source, double brightness, double contrast,
                                    double saturation, double temperature, double hue,
                                    double shadows, double highlights, double vignette)
{
    if (source.isNull())
        return source;

    QImage result = source.convertToFormat(QImage::Format_ARGB32);
    const int w = result.width();
    const int h = result.height();

    // Fatores normalizados
    const double bFactor = (brightness / 100.0) * 255.0; // [-255, 255]
    const double cFactor = (contrast >= 0)
        ? (1.0 + contrast / 100.0 * 2.0)
        : (1.0 + contrast / 100.0); // [0.0, 3.0]
    const double sFactor = 1.0 + saturation / 100.0; // [0.0, 2.0]
    const double tFactor = (temperature / 100.0) * 30.0; // [-30, 30]
    const bool hasHue = !qFuzzyIsNull(hue);

    for (int y = 0; y < h; ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < w; ++x) {
            const QRgb px = line[x];
            const int a = qAlpha(px);
            if (a == 0) continue;

            double r = qRed(px);
            double g = qGreen(px);
            double b = qBlue(px);

            const double luminance = (0.299 * r + 0.587 * g + 0.114 * b) / 255.0;
            const double shadowWeight = std::clamp(1.0 - luminance * 2.0, 0.0, 1.0);
            const double highlightWeight = std::clamp((luminance - 0.5) * 2.0, 0.0, 1.0);
            const double shadowOffset = (shadows / 100.0) * 90.0 * shadowWeight;
            const double highlightOffset = (highlights / 100.0) * 90.0 * highlightWeight;
            r += shadowOffset + highlightOffset;
            g += shadowOffset + highlightOffset;
            b += shadowOffset + highlightOffset;

            // 1. Brilho
            r += bFactor;
            g += bFactor;
            b += bFactor;

            // 2. Contraste (em torno de 128)
            r = (r - 128.0) * cFactor + 128.0;
            g = (g - 128.0) * cFactor + 128.0;
            b = (b - 128.0) * cFactor + 128.0;

            // 3. Balanço de Temperatura
            r += tFactor;
            b -= tFactor;

            // 4. Saturação
            const double gray = 0.299 * r + 0.587 * g + 0.114 * b;
            r = gray + (r - gray) * sFactor;
            g = gray + (g - gray) * sFactor;
            b = gray + (b - gray) * sFactor;

            int clampedR = std::clamp(static_cast<int>(std::round(r)), 0, 255);
            int clampedG = std::clamp(static_cast<int>(std::round(g)), 0, 255);
            int clampedB = std::clamp(static_cast<int>(std::round(b)), 0, 255);

            if (!qFuzzyIsNull(vignette)) {
                const double dx = (x - (w - 1) * 0.5) / (w * 0.5);
                const double dy = (y - (h - 1) * 0.5) / (h * 0.5);
                const double edge = std::clamp((dx * dx + dy * dy) * 0.5, 0.0, 1.0);
                const double factor = 1.0 - (vignette / 100.0) * edge;
                clampedR = std::clamp(static_cast<int>(std::round(clampedR * factor)), 0, 255);
                clampedG = std::clamp(static_cast<int>(std::round(clampedG * factor)), 0, 255);
                clampedB = std::clamp(static_cast<int>(std::round(clampedB * factor)), 0, 255);
            }

            // 5. Matiz / Tint (Hue Shift)
            if (hasHue) {
                QColor c = QColor::fromRgb(clampedR, clampedG, clampedB);
                float hVal = 0.0f;
                float sVal = 0.0f;
                float vVal = 0.0f;
                c.getHsvF(&hVal, &sVal, &vVal);
                if (hVal >= 0.0f) {
                    hVal += static_cast<float>(hue / 360.0);
                    while (hVal < 0.0f) hVal += 1.0f;
                    while (hVal >= 1.0f) hVal -= 1.0f;
                    c.setHsvF(hVal, sVal, vVal);
                    clampedR = c.red();
                    clampedG = c.green();
                    clampedB = c.blue();
                }
            }

            line[x] = qRgba(clampedR, clampedG, clampedB, a);
        }
    }

    return result;
}

QImage ImageProcessing::applyBlur(const QImage& source, double radius)
{
    if (source.isNull() || radius <= 0.1)
        return source;

    const int k = std::clamp(static_cast<int>(std::round(radius * 1.5)), 2, 30);
    QImage small = source.scaled(
        std::max(1, source.width() / k),
        std::max(1, source.height() / k),
        Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    return small.scaled(source.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

QImage ImageProcessing::applySharpen(const QImage& source, double amount)
{
    if (source.isNull() || amount <= 0.0)
        return source;

    QImage result = source.convertToFormat(QImage::Format_ARGB32);
    const QImage orig = result;
    const int w = result.width();
    const int h = result.height();
    const double factor = std::clamp(amount / 50.0, 0.1, 2.0);

    for (int y = 1; y < h - 1; ++y) {
        QRgb* dstLine = reinterpret_cast<QRgb*>(result.scanLine(y));
        const QRgb* prevLine = reinterpret_cast<const QRgb*>(orig.constScanLine(y - 1));
        const QRgb* currLine = reinterpret_cast<const QRgb*>(orig.constScanLine(y));
        const QRgb* nextLine = reinterpret_cast<const QRgb*>(orig.constScanLine(y + 1));

        for (int x = 1; x < w - 1; ++x) {
            const QRgb c = currLine[x];
            const QRgb n = prevLine[x];
            const QRgb s = nextLine[x];
            const QRgb w_px = currLine[x - 1];
            const QRgb e = currLine[x + 1];

            auto sharpChannel = [factor](int cv, int nv, int sv, int wv, int ev) -> int {
                const int lap = 4 * cv - nv - sv - wv - ev;
                return std::clamp(static_cast<int>(cv + lap * factor), 0, 255);
            };

            dstLine[x] = qRgba(sharpChannel(qRed(c), qRed(n), qRed(s), qRed(w_px), qRed(e)),
                               sharpChannel(qGreen(c), qGreen(n), qGreen(s), qGreen(w_px), qGreen(e)),
                               sharpChannel(qBlue(c), qBlue(n), qBlue(s), qBlue(w_px), qBlue(e)),
                               qAlpha(c));
        }
    }

    return result;
}

QImage ImageProcessing::applyLevels(const QImage& source, const LevelsChannel& master,
                                   const LevelsChannel& red, const LevelsChannel& green,
                                   const LevelsChannel& blue)
{
    if (source.isNull())
        return source;

    quint8 lutR[256];
    quint8 lutG[256];
    quint8 lutB[256];

    for (int i = 0; i < 256; ++i) {
        const float v = static_cast<float>(i) / 255.0f;
        // Channel level first, then master level (Photoshop convention)
        const float vr = red.isIdentity() ? v : ToneCurve::evaluateLevels(red, v);
        const float vg = green.isIdentity() ? v : ToneCurve::evaluateLevels(green, v);
        const float vb = blue.isIdentity() ? v : ToneCurve::evaluateLevels(blue, v);

        const float mr = master.isIdentity() ? vr : ToneCurve::evaluateLevels(master, vr);
        const float mg = master.isIdentity() ? vg : ToneCurve::evaluateLevels(master, vg);
        const float mb = master.isIdentity() ? vb : ToneCurve::evaluateLevels(master, vb);

        lutR[i] = static_cast<quint8>(std::round(mr * 255.0f));
        lutG[i] = static_cast<quint8>(std::round(mg * 255.0f));
        lutB[i] = static_cast<quint8>(std::round(mb * 255.0f));
    }

    QImage result = source.convertToFormat(QImage::Format_ARGB32);
    const int w = result.width();
    const int h = result.height();

    for (int y = 0; y < h; ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < w; ++x) {
            const QRgb px = line[x];
            const int a = qAlpha(px);
            if (a == 0) continue;
            line[x] = qRgba(lutR[qRed(px)], lutG[qGreen(px)], lutB[qBlue(px)], a);
        }
    }

    return result;
}

QImage ImageProcessing::applyCurves(const QImage& source, const std::vector<CurvePoint>& master,
                                   const std::vector<CurvePoint>& red,
                                   const std::vector<CurvePoint>& green,
                                   const std::vector<CurvePoint>& blue)
{
    if (source.isNull())
        return source;

    const std::vector<quint8> masterLut = ToneCurve::buildCurveLut(master);
    const std::vector<quint8> redLut = ToneCurve::buildCurveLut(red);
    const std::vector<quint8> greenLut = ToneCurve::buildCurveLut(green);
    const std::vector<quint8> blueLut = ToneCurve::buildCurveLut(blue);

    quint8 lutR[256];
    quint8 lutG[256];
    quint8 lutB[256];

    for (int i = 0; i < 256; ++i) {
        lutR[i] = masterLut[redLut[i]];
        lutG[i] = masterLut[greenLut[i]];
        lutB[i] = masterLut[blueLut[i]];
    }

    QImage result = source.convertToFormat(QImage::Format_ARGB32);
    const int w = result.width();
    const int h = result.height();

    for (int y = 0; y < h; ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < w; ++x) {
            const QRgb px = line[x];
            const int a = qAlpha(px);
            if (a == 0) continue;
            line[x] = qRgba(lutR[qRed(px)], lutG[qGreen(px)], lutB[qBlue(px)], a);
        }
    }

    return result;
}

QImage ImageProcessing::applyPresetFilter(const QImage& source, PresetFilter preset)
{
    if (source.isNull())
        return source;

    QImage result = source.convertToFormat(QImage::Format_ARGB32);
    const int w = result.width();
    const int h = result.height();

    for (int y = 0; y < h; ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < w; ++x) {
            const QRgb px = line[x];
            const int a = qAlpha(px);
            if (a == 0) continue;

            const int r = qRed(px);
            const int g = qGreen(px);
            const int b = qBlue(px);

            switch (preset) {
            case PresetFilter::Grayscale: {
                const int gray = qRound(0.299 * r + 0.587 * g + 0.114 * b);
                line[x] = qRgba(gray, gray, gray, a);
                break;
            }
            case PresetFilter::Sepia: {
                const int sr = std::clamp(qRound(0.393 * r + 0.769 * g + 0.189 * b), 0, 255);
                const int sg = std::clamp(qRound(0.349 * r + 0.686 * g + 0.168 * b), 0, 255);
                const int sb = std::clamp(qRound(0.272 * r + 0.534 * g + 0.131 * b), 0, 255);
                line[x] = qRgba(sr, sg, sb, a);
                break;
            }
            case PresetFilter::Vintage: {
                int vr = std::clamp(qRound(r * 1.1 + 15), 0, 255);
                int vg = std::clamp(qRound(g * 0.95), 0, 255);
                int vb = std::clamp(qRound(b * 0.8), 0, 255);
                line[x] = qRgba(vr, vg, vb, a);
                break;
            }
            case PresetFilter::HighContrast: {
                auto hc = [](int val) -> int {
                    const double norm = val / 255.0;
                    const double res = (norm < 0.5) ? (2.0 * norm * norm) : (1.0 - 2.0 * (1.0 - norm) * (1.0 - norm));
                    return std::clamp(qRound(res * 255.0), 0, 255);
                };
                line[x] = qRgba(hc(r), hc(g), hc(b), a);
                break;
            }
            case PresetFilter::Invert: {
                line[x] = qRgba(255 - r, 255 - g, 255 - b, a);
                break;
            }
            case PresetFilter::Posterize: {
                auto post = [](int val) -> int {
                    return std::clamp((val / 64) * 85, 0, 255);
                };
                line[x] = qRgba(post(r), post(g), post(b), a);
                break;
            }
            case PresetFilter::Threshold: {
                const int gray = qRound(0.299 * r + 0.587 * g + 0.114 * b);
                const int th = (gray >= 128) ? 255 : 0;
                line[x] = qRgba(th, th, th, a);
                break;
            }
            }
        }
    }

    return result;
}

QImage ImageProcessing::applyGradientMap(const QImage& source, const QColor& color1,
                                         const QColor& color2, bool reverse)
{
    if (source.isNull())
        return source;

    QImage result = source.convertToFormat(QImage::Format_ARGB32);
    const int w = result.width();
    const int h = result.height();

    const QColor cStart = reverse ? color2 : color1;
    const QColor cEnd = reverse ? color1 : color2;

    quint8 lutR[256];
    quint8 lutG[256];
    quint8 lutB[256];

    for (int i = 0; i < 256; ++i) {
        const float t = i / 255.0f;
        lutR[i] = static_cast<quint8>(std::round(cStart.redF() * (1.0f - t) * 255.0f + cEnd.redF() * t * 255.0f));
        lutG[i] = static_cast<quint8>(std::round(cStart.greenF() * (1.0f - t) * 255.0f + cEnd.greenF() * t * 255.0f));
        lutB[i] = static_cast<quint8>(std::round(cStart.blueF() * (1.0f - t) * 255.0f + cEnd.blueF() * t * 255.0f));
    }

    for (int y = 0; y < h; ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < w; ++x) {
            const QRgb px = line[x];
            const int a = qAlpha(px);
            if (a == 0) continue;
            const int lum = std::clamp(qRound(0.299 * qRed(px) + 0.587 * qGreen(px) + 0.114 * qBlue(px)), 0, 255);
            line[x] = qRgba(lutR[lum], lutG[lum], lutB[lum], a);
        }
    }

    return result;
}

QImage ImageProcessing::colorToAlpha(const QImage& source, const QColor& keyColor,
                                     double transparency, double opacityThreshold,
                                     double tolerance)
{
    if (source.isNull())
        return {};

    QImage result = source.convertToFormat(QImage::Format_ARGB32);
    const int w = result.width();
    const int h = result.height();

    const double cr = keyColor.redF();
    const double cg = keyColor.greenF();
    const double cb = keyColor.blueF();

    const double t = std::clamp(transparency, 0.0, 1.0);
    const double o = std::clamp(opacityThreshold, 0.0, 1.0);
    const double tol = std::clamp(tolerance, 0.0, 1.0);

    for (int y = 0; y < h; ++y) {
        auto* scanLine = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < w; ++x) {
            const QRgb px = scanLine[x];
            const double origA = qAlpha(px) / 255.0;
            if (origA <= 0.0)
                continue;

            const double pr = qRed(px) / 255.0;
            const double pg = qGreen(px) / 255.0;
            const double pb = qBlue(px) / 255.0;

            // Opacity requirement per channel i:
            // if P_i > C_i: ai = (P_i - C_i) / (1 - C_i)
            // if P_i < C_i: ai = (C_i - P_i) / C_i
            // if P_i == C_i: ai = 0
            auto channelOpacity = [](double p, double c) -> double {
                if (p > c) {
                    return (c < 1.0) ? ((p - c) / (1.0 - c)) : 1.0;
                } else if (p < c) {
                    return (c > 0.0) ? ((c - p) / c) : 1.0;
                }
                return 0.0;
            };

            const double ar = channelOpacity(pr, cr);
            const double ag = channelOpacity(pg, cg);
            const double ab = channelOpacity(pb, cb);

            double a = std::max({ar, ag, ab});

            // Se tolerance > 0, calcula a distância cromática (Euclidean distance)
            if (tol > 0.0) {
                const double dist = std::sqrt((pr - cr) * (pr - cr) +
                                              (pg - cg) * (pg - cg) +
                                              (pb - cb) * (pb - cb)) / 1.7320508; // normalizado [0, 1]
                if (dist <= tol) {
                    a = 0.0;
                } else if (dist < tol + 0.15) {
                    const double factor = (dist - tol) / 0.15;
                    a = std::min(a, factor);
                }
            }

            a = std::clamp(a, 0.0, 1.0);

            // Remap thresholds: a <= transparency -> 0, a >= opacityThreshold -> 1, linear between
            if (a <= t) {
                a = 0.0;
            } else if (a >= o) {
                a = 1.0;
            } else if (o > t) {
                a = (a - t) / (o - t);
                a = std::clamp(a, 0.0, 1.0);
            }

            double qr = pr;
            double qg = pg;
            double qb = pb;

            if (a < 1.0) {
                if (a <= 0.0) {
                    qr = cr;
                    qg = cg;
                    qb = cb;
                } else {
                    // Desmistura da cor chave com salvaguarda contra saturação exagerada
                    qr = std::clamp(cr + (pr - cr) / a, 0.0, 1.0);
                    qg = std::clamp(cg + (pg - cg) / a, 0.0, 1.0);
                    qb = std::clamp(cb + (pb - cb) / a, 0.0, 1.0);

                    // Spill suppression: se a cor chave era predominantemente verde, limita o verde
                    // residual das bordas sem deixar estourar em roxo intenso.
                    if (cg > cr + 0.15 && cg > cb + 0.15) {
                        const double maxAllowedG = std::max(qr, qb);
                        if (qg > maxAllowedG) {
                            qg = maxAllowedG;
                        }
                    } else if (cb > cr + 0.15 && cb > cg + 0.15) {
                        const double maxAllowedB = std::max(qr, qg);
                        if (qb > maxAllowedB) {
                            qb = maxAllowedB;
                        }
                    }
                }
            }

            const double finalAlpha = origA * a;
            scanLine[x] = qRgba(
                std::clamp(static_cast<int>(std::round(qr * 255.0)), 0, 255),
                std::clamp(static_cast<int>(std::round(qg * 255.0)), 0, 255),
                std::clamp(static_cast<int>(std::round(qb * 255.0)), 0, 255),
                std::clamp(static_cast<int>(std::round(finalAlpha * 255.0)), 0, 255)
            );
        }
    }

    return result;
}

// ============================================================================
// Ferramentas de preenchimento, seleção e retoque (Photoshop-like)
// ============================================================================

namespace {

// Blur separável de caixa sobre Alpha8 (feather de seleção). Determinístico.
void boxBlurAlpha8(QImage& alpha, int radius)
{
    if (alpha.isNull() || radius <= 0)
        return;
    alpha = alpha.convertToFormat(QImage::Format_Alpha8);
    const int w = alpha.width();
    const int h = alpha.height();
    const int r = std::min({radius, w / 2, h / 2});
    if (r <= 0)
        return;

    std::vector<quint16> tmp(static_cast<size_t>(w) * h);
    // Horizontal
    for (int y = 0; y < h; ++y) {
        const uchar* row = alpha.constScanLine(y);
        for (int x = 0; x < w; ++x) {
            int sum = 0, count = 0;
            const int x0 = std::max(0, x - r);
            const int x1 = std::min(w - 1, x + r);
            for (int sx = x0; sx <= x1; ++sx) { sum += row[sx]; ++count; }
            tmp[static_cast<size_t>(y) * w + x] = static_cast<quint16>(sum / std::max(1, count));
        }
    }
    // Vertical
    for (int y = 0; y < h; ++y) {
        uchar* row = alpha.scanLine(y);
        for (int x = 0; x < w; ++x) {
            int sum = 0, count = 0;
            const int y0 = std::max(0, y - r);
            const int y1 = std::min(h - 1, y + r);
            for (int sy = y0; sy <= y1; ++sy) { sum += tmp[static_cast<size_t>(sy) * w + x]; ++count; }
            row[x] = static_cast<uchar>(sum / std::max(1, count));
        }
    }
}

inline qreal falloff(qreal dist, qreal radius)
{
    if (radius <= 0.0)
        return 0.0;
    const qreal t = std::clamp(1.0 - dist / radius, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t); // smoothstep
}

inline int clampByte(qreal v)
{
    return std::clamp(static_cast<int>(std::lround(v)), 0, 255);
}

inline QColor unpremul(const QImage& img, int x, int y)
{
    return QColor::fromRgba(qUnpremultiply(img.pixel(x, y)));
}

inline void writeColor(QImage& img, int x, int y, qreal r, qreal g, qreal b, int a)
{
    QRgb* row = reinterpret_cast<QRgb*>(img.scanLine(y));
    row[x] = qPremultiply(qRgba(clampByte(r), clampByte(g), clampByte(b),
                                std::clamp(a, 0, 255)));
}

} // namespace

QImage ImageProcessing::makeShapeSelection(const QSize& size, const QRectF& rect,
                                           bool ellipse, int feather)
{
    if (size.isEmpty())
        return {};
    QImage selection(size, QImage::Format_Alpha8);
    selection.fill(0);
    const QRectF area = rect.normalized().intersected(
        QRectF(QPointF(0, 0), QSizeF(size)));
    if (area.isEmpty() || area.width() < 0.5 || area.height() < 0.5)
        return selection;

    {
        QPainter p(&selection);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        if (ellipse)
            p.drawEllipse(area);
        else
            p.fillRect(area, Qt::white);
    }
    if (feather > 0)
        boxBlurAlpha8(selection, feather);
    return selection;
}

QImage ImageProcessing::combineSelections(const QImage& base, const QImage& overlay,
                                          bool subtract)
{
    if (base.isNull())
        return overlay;
    if (overlay.isNull())
        return base;
    if (base.size() != overlay.size())
        return base.convertToFormat(QImage::Format_Alpha8);

    QImage out = base.convertToFormat(QImage::Format_Alpha8);
    const QImage ov = overlay.convertToFormat(QImage::Format_Alpha8);
    for (int y = 0; y < out.height(); ++y) {
        uchar* dst = out.scanLine(y);
        const uchar* src = ov.constScanLine(y);
        for (int x = 0; x < out.width(); ++x) {
            if (subtract)
                dst[x] = static_cast<uchar>(std::max(0, int(dst[x]) - int(src[x])));
            else
                dst[x] = std::max(dst[x], src[x]);
        }
    }
    return out;
}

QImage ImageProcessing::paintGradient(const QImage& target, const QPointF& start,
                                      const QPointF& end, const QColor& startColor,
                                      const QColor& endColor, bool radial,
                                      qreal opacity, const QImage& mask)
{
    if (target.isNull() || opacity <= 0.0)
        return target;

    QImage gradient(target.size(), QImage::Format_ARGB32_Premultiplied);
    gradient.fill(Qt::transparent);
    {
        QPainter gp(&gradient);
        gp.setRenderHint(QPainter::Antialiasing, true);
        gp.setPen(Qt::NoPen);
        if (radial) {
            const qreal radius = std::max<qreal>(1.0, QLineF(start, end).length());
            QRadialGradient g(start, radius);
            g.setColorAt(0.0, startColor);
            g.setColorAt(1.0, endColor);
            gp.setBrush(QBrush(g));
        } else {
            QPointF s = start, e = end;
            if (QLineF(s, e).length() < 0.5)
                e = s + QPointF(1.0, 0.0);
            QLinearGradient g(s, e);
            g.setColorAt(0.0, startColor);
            g.setColorAt(1.0, endColor);
            gp.setBrush(QBrush(g));
        }
        gp.drawRect(gradient.rect());
    }

    if (!mask.isNull() && mask.size() == gradient.size()) {
        const QImage alpha = mask.convertToFormat(QImage::Format_Alpha8);
        for (int y = 0; y < gradient.height(); ++y) {
            QRgb* row = reinterpret_cast<QRgb*>(gradient.scanLine(y));
            const uchar* mrow = alpha.constScanLine(y);
            for (int x = 0; x < gradient.width(); ++x) {
                const uchar ma = mrow[x];
                if (ma >= 255)
                    continue;
                const QRgb px = row[x];
                const int a = qAlpha(px) * ma / 255;
                row[x] = qRgba(qRed(px), qGreen(px), qBlue(px), a);
            }
        }
    }

    QImage result = target.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QPainter p(&result);
    p.setOpacity(std::clamp(opacity, 0.0, 1.0));
    p.drawImage(0, 0, gradient);
    p.end();
    return result;
}

QImage ImageProcessing::retouchStroke(const QImage& target, const QPointF& prevPoint,
                                      const QPointF& curPoint, RetouchMode mode,
                                      int size, qreal strength)
{
    if (target.isNull() || size <= 0)
        return target;

    const qreal radius = std::max<qreal>(0.5, size / 2.0);
    const qreal blend = std::clamp(strength, 0.01, 1.0);

    QImage result = target.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const QImage snapshot = result; // estabiliza leituras do próprio traço

    const QPointF delta = curPoint - prevPoint;
    const qreal distance = std::hypot(delta.x(), delta.y());
    const qreal stepPx = std::max<qreal>(1.0, radius * 0.35);
    const int steps = std::max(1, static_cast<int>(std::ceil(distance / stepPx)));

    const int w = result.width();
    const int h = result.height();
    const int iradius = static_cast<int>(std::ceil(radius));
    // Para Smudge, a origem é o ponto anterior deslocado.
    const QPointF smudgeOffset = prevPoint - curPoint;

    for (int s = 1; s <= steps; ++s) {
        const QPointF center = prevPoint + delta * (static_cast<qreal>(s) / steps);
        const int cx = static_cast<int>(std::floor(center.x()));
        const int cy = static_cast<int>(std::floor(center.y()));
        const int x0 = std::max(0, cx - iradius);
        const int y0 = std::max(0, cy - iradius);
        const int x1 = std::min(w - 1, cx + iradius);
        const int y1 = std::min(h - 1, cy + iradius);
        if (x0 > x1 || y0 > y1)
            continue;

        for (int y = y0; y <= y1; ++y) {
            QRgb* row = reinterpret_cast<QRgb*>(result.scanLine(y));
            for (int x = x0; x <= x1; ++x) {
                const qreal d = std::hypot(qreal(x) + 0.5 - center.x(),
                                           qreal(y) + 0.5 - center.y());
                const qreal wgt = falloff(d, radius);
                if (wgt <= 0.001)
                    continue;
                const qreal t = wgt * blend;

                const QRgb dpx = qUnpremultiply(row[x]);
                qreal dr = qRed(dpx), dg = qGreen(dpx), db = qBlue(dpx);
                const int da = qAlpha(dpx);

                qreal tr = dr, tg = dg, tb = db;
                switch (mode) {
                case RetouchMode::Blur: {
                    // média de 8 vizinhos distantes (aproxima gaussiana)
                    const int off = std::max(1, iradius / 3);
                    qreal sr = 0, sg = 0, sb = 0; int n = 0;
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            const int sx = std::clamp(x + dx * off, 0, w - 1);
                            const int sy = std::clamp(y + dy * off, 0, h - 1);
                            if (sx == x && sy == y) continue;
                            const QRgb sp = qUnpremultiply(snapshot.pixel(sx, sy));
                            sr += qRed(sp); sg += qGreen(sp); sb += qBlue(sp); ++n;
                        }
                    }
                    if (n > 0) { tr = sr / n; tg = sg / n; tb = sb / n; }
                    break;
                }
                case RetouchMode::Sharpen: {
                    const int off = std::max(1, iradius / 3);
                    qreal sr = 0, sg = 0, sb = 0; int n = 0;
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            const int sx = std::clamp(x + dx * off, 0, w - 1);
                            const int sy = std::clamp(y + dy * off, 0, h - 1);
                            if (sx == x && sy == y) continue;
                            const QRgb sp = qUnpremultiply(snapshot.pixel(sx, sy));
                            sr += qRed(sp); sg += qGreen(sp); sb += qBlue(sp); ++n;
                        }
                    }
                    if (n > 0) {
                        const qreal amount = 1.2;
                        tr = std::clamp(dr + (dr - sr / n) * amount, 0.0, 255.0);
                        tg = std::clamp(dg + (dg - sg / n) * amount, 0.0, 255.0);
                        tb = std::clamp(db + (db - sb / n) * amount, 0.0, 255.0);
                    }
                    break;
                }
                case RetouchMode::Smudge: {
                    const int sx = std::clamp(static_cast<int>(std::lround(x + smudgeOffset.x())), 0, w - 1);
                    const int sy = std::clamp(static_cast<int>(std::lround(y + smudgeOffset.y())), 0, h - 1);
                    const QRgb sp = qUnpremultiply(snapshot.pixel(sx, sy));
                    tr = qRed(sp); tg = qGreen(sp); tb = qBlue(sp);
                    break;
                }
                case RetouchMode::Dodge: {
                    const qreal k = std::min<qreal>(1.0, t * 1.6);
                    tr = dr + (255.0 - dr) * k;
                    tg = dg + (255.0 - dg) * k;
                    tb = db + (255.0 - db) * k;
                    break;
                }
                case RetouchMode::Burn: {
                    const qreal k = std::min<qreal>(1.0, t * 1.4);
                    tr = dr * (1.0 - k);
                    tg = dg * (1.0 - k);
                    tb = db * (1.0 - k);
                    break;
                }
                case RetouchMode::Sponge: {
                    const qreal lum = 0.299 * dr + 0.587 * dg + 0.114 * db;
                    tr = dr + (lum - dr) * t;
                    tg = dg + (lum - dg) * t;
                    tb = db + (lum - db) * t;
                    break;
                }
                }

                // Mistura final (fonte = tr/tg/tb, alpha = dst alpha)
                row[x] = qPremultiply(qRgba(
                    clampByte(dr + (tr - dr) * t),
                    clampByte(dg + (tg - dg) * t),
                    clampByte(db + (tb - db) * t),
                    da));
            }
        }
    }
    return result.convertToFormat(target.format());
}

QImage ImageProcessing::spotHeal(const QImage& target, const QPoint& targetPoint,
                                 const QPoint& sourcePoint, int radius,
                                 qreal strength)
{
    if (target.isNull() || radius <= 0)
        return target;

    const int w = target.width(), h = target.height();
    const int iradius = std::max(1, radius);
    const qreal blend = std::clamp(strength, 0.05, 1.0);

    QImage result = target.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const QImage snapshot = result;

    const int x0 = std::max(0, targetPoint.x() - iradius);
    const int y0 = std::max(0, targetPoint.y() - iradius);
    const int x1 = std::min(w - 1, targetPoint.x() + iradius);
    const int y1 = std::min(h - 1, targetPoint.y() + iradius);
    if (x0 > x1 || y0 > y1)
        return target;

    // Transferência de cor: compara médias da origem e do destino na região
    // para o patch não destoar do entorno (look de "healing").
    qreal dR = 0, dG = 0, dB = 0, sR = 0, sG = 0, sB = 0;
    int count = 0;
    for (int y = y0; y <= y1; y += 2) {
        for (int x = x0; x <= x1; x += 2) {
            const QRgb dst = qUnpremultiply(snapshot.pixel(x, y));
            const int sx = std::clamp(x + (sourcePoint.x() - targetPoint.x()), 0, w - 1);
            const int sy = std::clamp(y + (sourcePoint.y() - targetPoint.y()), 0, h - 1);
            const QRgb src = qUnpremultiply(snapshot.pixel(sx, sy));
            dR += qRed(dst); dG += qGreen(dst); dB += qBlue(dst);
            sR += qRed(src); sG += qGreen(src); sB += qBlue(src);
            ++count;
        }
    }
    const qreal offR = count ? (dR - sR) / count : 0.0;
    const qreal offG = count ? (dG - sG) / count : 0.0;
    const qreal offB = count ? (dB - sB) / count : 0.0;

    for (int y = y0; y <= y1; ++y) {
        QRgb* row = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = x0; x <= x1; ++x) {
            const qreal d = std::hypot(qreal(x - targetPoint.x()),
                                       qreal(y - targetPoint.y()));
            const qreal wgt = falloff(d, iradius) * blend;
            if (wgt <= 0.001)
                continue;
            const int sx = std::clamp(x + (sourcePoint.x() - targetPoint.x()), 0, w - 1);
            const int sy = std::clamp(y + (sourcePoint.y() - targetPoint.y()), 0, h - 1);
            const QRgb src = qUnpremultiply(snapshot.pixel(sx, sy));
            const QRgb dst = qUnpremultiply(row[x]);
            const qreal sr = std::clamp(qRed(src) + offR, 0.0, 255.0);
            const qreal sg = std::clamp(qGreen(src) + offG, 0.0, 255.0);
            const qreal sb = std::clamp(qBlue(src) + offB, 0.0, 255.0);
            row[x] = qPremultiply(qRgba(
                clampByte(qRed(dst) + (sr - qRed(dst)) * wgt),
                clampByte(qGreen(dst) + (sg - qGreen(dst)) * wgt),
                clampByte(qBlue(dst) + (sb - qBlue(dst)) * wgt),
                qAlpha(dst)));
        }
    }
    return result.convertToFormat(target.format());
}

QImage ImageProcessing::redEyeRemove(const QImage& target, const QPoint& center, int radius)
{
    if (target.isNull() || radius <= 0)
        return target;

    QImage result = target.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const int w = result.width(), h = result.height();
    const int x0 = std::max(0, center.x() - radius);
    const int y0 = std::max(0, center.y() - radius);
    const int x1 = std::min(w - 1, center.x() + radius);
    const int y1 = std::min(h - 1, center.y() + radius);
    if (x0 > x1 || y0 > y1)
        return target;

    for (int y = y0; y <= y1; ++y) {
        QRgb* row = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = x0; x <= x1; ++x) {
            const qreal d = std::hypot(qreal(x - center.x()), qreal(y - center.y()));
            const qreal wgt = falloff(d, radius);
            if (wgt <= 0.001)
                continue;
            const QRgb dst = qUnpremultiply(row[x]);
            const int r = qRed(dst), g = qGreen(dst), b = qBlue(dst);
            // Pixels claramente "avermelhados" (flash de olho vermelho)
            const bool reddish = r > 90 && r > g * 1.35 + 10 && r > b * 1.35 + 10;
            if (!reddish)
                continue;
            const qreal neutral = (g + b) * 0.5;
            const qreal nr = r + (neutral - r) * wgt;
            row[x] = qPremultiply(qRgba(clampByte(nr), g, b, qAlpha(dst)));
        }
    }
    return result.convertToFormat(target.format());
}

namespace {
// Ordena os pontos em sentido horário visual (y cresce para baixo),
// começando pelo ponto mais próximo do canto superior-esquerdo — a
// convenção TL→TR→BR→BL que o QTransform::quadToQuad espera. Genérico
// para qualquer anel de 3+ pontos.
QPolygonF mockupRingOrder(const QPolygonF& corners)
{
    if (corners.size() < 3)
        return corners;

    QPointF centroid(0, 0);
    for (const QPointF& p : corners)
        centroid += p;
    centroid /= qreal(corners.size());

    QVector<QPointF> sorted;
    for (const QPointF& p : corners)
        sorted.append(p);
    std::sort(sorted.begin(), sorted.end(),
              [&centroid](const QPointF& a, const QPointF& b) {
                  const double aa = qAtan2(a.y() - centroid.y(),
                                           a.x() - centroid.x());
                  const double ab = qAtan2(b.y() - centroid.y(),
                                           b.x() - centroid.x());
                  if (aa != ab)
                      return aa < ab;
                  // Empate angular (dois pontos no MESMO raio do centróide,
                  // ex.: vértice côncavo no meio da parede): o passeio CW
                  // vai sempre do mais distante ao mais próximo no lado
                  // direito do centróide, e o contrário no lado esquerdo.
                  // Empates arbitrários criam anéis em zigue-zague e furam
                  // o warp por forma.
                  const double da = (a.x() - centroid.x()) * (a.x() - centroid.x())
                                  + (a.y() - centroid.y()) * (a.y() - centroid.y());
                  const double db = (b.x() - centroid.x()) * (b.x() - centroid.x())
                                  + (b.y() - centroid.y()) * (b.y() - centroid.y());
                  if (qCos(aa) >= 0.0)
                      return da > db; // lado direito: mais longe primeiro
                  return da < db;     // lado esquerdo: mais perto primeiro
              });

    // O passeio pela borda da fonte começa no canto TL (0,0): o anel deve
    // começar no ponto mais "acima-esquerda" VISTO DO CENTRÓIDE (direção
    // −135° em coords de tela). min(x+y) falhava em áreas que alargam para
    // baixo (estrada em perspectiva): escolhia a base como início e o
    // conteúdo aparecia como se a base fosse o "ponto 1".
    int start = 0;
    double best = std::numeric_limits<double>::max();
    for (int i = 0; i < sorted.size(); ++i) {
        const double ang = qAtan2(sorted[i].y() - centroid.y(),
                                  sorted[i].x() - centroid.x());
        double diff = std::abs(ang + 3.0 * M_PI / 4.0);
        if (diff > M_PI)
            diff = 2.0 * M_PI - diff;
        if (diff < best) {
            best = diff;
            start = i;
        }
    }
    QPolygonF normalized;
    for (int i = 0; i < sorted.size(); ++i)
        normalized.append(sorted[(start + i) % sorted.size()]);
    return normalized;
}

// O quadToQuad só é confiável quando o quad é CONVEXO e simples: cliques
// em zigue-zague/interior produzem um quad côncavo ou "laço" e o warp
// vira uma fita degenerada. Nesses casos cai no MLS (mais esperto).
bool mockupQuadIsConvex(const QPolygonF& quad)
{
    if (quad.size() != 4)
        return false;
    auto cross = [](const QPointF& o, const QPointF& a, const QPointF& b) {
        return (a.x() - o.x()) * (b.y() - o.y())
             - (a.y() - o.y()) * (b.x() - o.x());
    };
    double sign = 0.0;
    for (int i = 0; i < 4; ++i) {
        const double c = cross(quad[i], quad[(i + 1) % 4], quad[(i + 2) % 4]);
        if (std::abs(c) < 1e-9)
            continue;
        const double s = (c > 0.0) ? 1.0 : -1.0;
        if (sign == 0.0)
            sign = s;
        else if (sign != s)
            return false;
    }
    return true;
}

} // namespace

// Warp de mockup estilo Photoshop: quad projetivo com 4 pontos; nos
// demais casos (5+ pontos ou quad côncavo), warp por FORMA — o polígono
// marcado molda a imagem inteira linha por linha, sem rotação/recorte.
MockupWarpResult
ImageProcessing::mockupWarp(const QImage& source, const QPolygonF& destinationPoints)
{
    MockupWarpResult result;
    if (source.isNull() || destinationPoints.size() < 4)
        return result;

    // Métrica: bounding box dos pontos marcados.
    qreal x0 = destinationPoints.first().x();
    qreal y0 = destinationPoints.first().y();
    qreal x1 = x0, y1 = y0;
    for (const QPointF& p : destinationPoints) {
        x0 = qMin(x0, p.x()); y0 = qMin(y0, p.y());
        x1 = qMax(x1, p.x()); y1 = qMax(y1, p.y());
    }
    const int outW = qMax(1, qRound(x1 - x0));
    const int outH = qMax(1, qRound(y1 - y0));

    const int n = source.width();
    const int m = source.height();
    const QImage src = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    // Margem: a deformação pode estender o conteúdo um pouco além da área
    // marcada (por exemplo quando os pontos não reproduzem exatamente a
    // proporção da fonte). Como o Photoshop, NADA é recortado: a camada
    // ganha uma margem transparente e mostra a imagem deformada inteira.
    const int pad = qRound(qMax<qreal>(2.0, 0.02 * qMax<qreal>(outW, outH)));
    const int canvasW = qMax(1, outW + 2 * pad);
    const int canvasH = qMax(1, outH + 2 * pad);
    result.image = QImage(canvasW, canvasH, QImage::Format_ARGB32_Premultiplied);
    result.image.fill(Qt::transparent);

    // Anel de pinos em coordenadas locais (bbox em 0,0). Pontos duplicados
    // (clique duplo acidental, mesmo ponto duas vezes) são removidos: um
    // vértice repetido criaria arcos degenerados (um lado com um único
    // ponto) e derrubaria o warp no fallback.
    QPolygonF localPts;
    localPts.reserve(destinationPoints.size());
    for (const QPointF& p : destinationPoints) {
        const QPointF lp(p.x() - x0, p.y() - y0);
        bool dup = false;
        for (const QPointF& q : localPts)
            dup = dup || std::hypot(q.x() - lp.x(), q.y() - lp.y()) < 1e-6;
        if (!dup)
            localPts.append(lp);
    }

    // Anel dos pontos em ordem canônica (CW iniciando no acima-esquerda) —
    // o resultado não depende da ordem em que o usuário clicou.
    QPolygonF ring = mockupRingOrder(localPts);
    const bool useProjective = (ring.size() == 4 && mockupQuadIsConvex(ring));

    // Amostragem bilinear com clamp nas bordas da fonte.
    auto sampleBilinear = [&](double sx, double sy) -> QRgb {
        const int ix = qFloor(sx);
        const int iy = qFloor(sy);
        const double fx = sx - qreal(ix);
        const double fy = sy - qreal(iy);
        const int ix1 = qMin(ix + 1, n - 1);
        const int iy1 = qMin(iy + 1, m - 1);
        const QRgb c00 = src.pixel(ix, iy);
        const QRgb c10 = src.pixel(ix1, iy);
        const QRgb c01 = src.pixel(ix, iy1);
        const QRgb c11 = src.pixel(ix1, iy1);
        const double iw = 1.0 - fx;
        const double jw = 1.0 - fy;
        const double r = qRed(c00) * iw * jw + qRed(c10) * fx * jw
                       + qRed(c01) * iw * fy + qRed(c11) * fx * fy;
        const double g = qGreen(c00) * iw * jw + qGreen(c10) * fx * jw
                       + qGreen(c01) * iw * fy + qGreen(c11) * fx * fy;
        const double b = qBlue(c00) * iw * jw + qBlue(c10) * fx * jw
                       + qBlue(c01) * iw * fy + qBlue(c11) * fx * fy;
        const double a = qAlpha(c00) * iw * jw + qAlpha(c10) * fx * jw
                       + qAlpha(c01) * iw * fy + qAlpha(c11) * fx * fy;
        return qRgba(qRound(r), qRound(g), qRound(b), qRound(a));
    };

    if (useProjective) {
        // ====================================================================
        // Quad: warp projetivo exato via quadToQuad + amostragem bilinear
        // por pixel (Photoshop Perspective). Os 4 cantos da fonte vão
        // exatamente aos 4 pontos marcados; conteúdo além do quad mapeia
        // para fora da fonte e fica transparente naturalmente.
        // ====================================================================
        QPolygonF srcQuad;
        srcQuad << QPointF(0, 0) << QPointF(n, 0)
                << QPointF(n, m) << QPointF(0, m);
        QTransform warp;
        if (!QTransform::quadToQuad(srcQuad, ring, warp)) {
            result.image = QImage();
            return result;
        }
        const QTransform invWarp = warp.inverted();

        for (int y = 0; y < canvasH; ++y) {
            QRgb* dstLine = reinterpret_cast<QRgb*>(result.image.scanLine(y));
            for (int x = 0; x < canvasW; ++x) {
                const QPointF srcPt = invWarp.map(QPointF(qreal(x - pad),
                                                         qreal(y - pad)));
                if (srcPt.x() < 0.0 || srcPt.x() >= n
                    || srcPt.y() < 0.0 || srcPt.y() >= m)
                    continue;
                dstLine[x] = sampleBilinear(srcPt.x(), srcPt.y());
            }
        }
    } else {
        // ====================================================================
        // Warp por FORMA (patch de Coons, "transfinite interpolation"):
        //  * as 4 PONTAS da imagem vão exatamente aos 4 CANTOS da área
        //    marcada — o conteúdo permanece centralizado;
        //  * os pontos do meio de cada aresta dobram a curva daquela borda
        //    (para dentro cria a perspectiva) — a imagem inteira acompanha
        //    suavemente, sem rotação global e sem recorte;
        //  * interior: P(u,v) = Coons(T,B,L,R); inversão por Newton pixel
        //    a pixel + amostragem bilinear da fonte por (u,v).
        // ====================================================================
        const int k = ring.size();

        // ====================================================================
        // Seleção dos 4 cantos entre os k pontos (k pode ser > 4 e incluir
        // pontos INTERNOS — ex.: um pino no topo de uma placa STOP).
        //
        // Ranking GULOSO por diagonal: a diagonal 1 (x+y mínimo/máximo) pega
        // os seus extremos e a diagonal 2 (x−y) pega os extremos ainda NÃO
        // usados. Um ranking independente (como o anterior) poderia dar o
        // MESMO ponto para TL e TR quando um pino interno vence os dois
        // rankings — cornersOk=false e a imagem caía no fallback de
        // retângulo, IGNORANDO todos os pontos.
        //
        // Os extremos são SÓ candidatos: o canto inicial (TL) é o candidato
        // mais próximo da direção −135° vista do centróide (mesmo critério
        // do anel e da prévia). Escolher TL por min(x+y) invertia a imagem
        // 180° em áreas que alargam para baixo (estrada em perspectiva: o
        // canto inferior-esquerdo tinha x+y menor que o superior-esquerdo).
        // ====================================================================
        int iTL = -1, iTR = -1, iBR = -1, iBL = -1;
        {
            double sMin = std::numeric_limits<double>::max();
            double sMax = -std::numeric_limits<double>::max();
            int iA = -1, iB = -1;                          // diagonal 1
            for (int i = 0; i < k; ++i) {
                const double s = ring[i].x() + ring[i].y();
                if (s < sMin) { sMin = s; iA = i; }
                if (s > sMax) { sMax = s; iB = i; }
            }
            // Diagonal 2: extremos de x−y entre os pontos ainda livres.
            double tMax = -std::numeric_limits<double>::max();
            double tMin = std::numeric_limits<double>::max();
            for (int i = 0; i < k; ++i) {
                if (i == iA || i == iB)
                    continue;
                const double t = ring[i].x() - ring[i].y();
                if (t > tMax) { tMax = t; iTR = i; }
                if (t < tMin) { tMin = t; iBL = i; }
            }
            iTL = iA;
            iBR = iB;
            // TL real: candidato mais perto de −135° do centróide do anel.
            QPointF centroid(0, 0);
            for (const QPointF& p : ring)
                centroid += p;
            centroid /= qreal(k);
            double bestAng = -1.0;
            for (int c : {iA, iB, iTR, iBL}) {
                if (c < 0)
                    continue;
                const double ang = std::atan2(ring[c].y() - centroid.y(),
                                              ring[c].x() - centroid.x());
                double diff = std::abs(ang + 3.0 * M_PI / 4.0);
                if (diff > M_PI)
                    diff = 2.0 * M_PI - diff;
                if (bestAng < 0.0 || diff < bestAng) {
                    bestAng = diff;
                    iTL = c;
                }
            }
        }
        const bool cornersOk = iTL >= 0 && iTR >= 0 && iBR >= 0 && iBL >= 0
                            && iTL != iTR && iTL != iBR && iTL != iBL
                            && iTR != iBR && iTR != iBL && iBR != iBL;

        // Rotaciona o anel para começar no canto TL (o anel já é CW).
        QPolygonF ordered;
        if (cornersOk) {
            ordered.reserve(k);
            for (int i = 0; i < k; ++i)
                ordered.append(ring[(iTL + i) % k]);
            // Reetiqueta TR/BR/BL pela posição no anel: o passeio CW visto
            // do TL passa pelo acima-direita (TR), depois pelo abaixo-
            // direita (BR) e por último pelo abaixo-esquerda (BL).
            QVector<int> others{iTR, iBR, iBL};
            std::sort(others.begin(), others.end(), [&](int p, int q) {
                return (p - iTL + k) % k < (q - iTL + k) % k;
            });
            iTR = others[0];
            iBR = others[1];
            iBL = others[2];
        }
        const int posTR = cornersOk ? (iTR - iTL + k) % k : -1;
        const int posBR = cornersOk ? (iBR - iTL + k) % k : -1;
        const int posBL = cornersOk ? (iBL - iTL + k) % k : -1;
        const bool orderOk = cornersOk && posTR > 0
                          && posTR < posBR && posBR < posBL;

        // Plano B (anel degenerado): estica a imagem inteira reta ao bbox —
        // previsível, sem rotação e sem recorte.
        auto stretchBox = [&]() {
            const QRectF localBox = QPolygonF(localPts).boundingRect();
            const QPolygonF boxQuad{
                localBox.topLeft(), localBox.topRight(),
                localBox.bottomRight(), localBox.bottomLeft()};
            QPolygonF srcQuad;
            srcQuad << QPointF(0, 0) << QPointF(n, 0)
                    << QPointF(n, m) << QPointF(0, m);
            QTransform warp;
            if (!QTransform::quadToQuad(srcQuad, boxQuad, warp))
                return;
            const QTransform invWarp = warp.inverted();
            for (int y = 0; y < canvasH; ++y) {
                QRgb* dstLine = reinterpret_cast<QRgb*>(result.image.scanLine(y));
                for (int x = 0; x < canvasW; ++x) {
                    const QPointF srcPt = invWarp.map(QPointF(qreal(x - pad),
                                                             qreal(y - pad)));
                    if (srcPt.x() < 0.0 || srcPt.x() >= n
                        || srcPt.y() < 0.0 || srcPt.y() >= m)
                        continue;
                    dstLine[x] = sampleBilinear(srcPt.x(), srcPt.y());
                }
            }
        };

        if (!cornersOk || !orderOk) {
            stretchBox();
        } else {
            // Arestas do anel entre os cantos: o passeio CW de TL vai a
            // TR (topo), TR a BR (direita), BR a BL (base) e BL volta a
            // TL (esquerda).
            auto arc = [&](int from, int to) {
                QPolygonF side;
                for (int i = from; i <= to; ++i)
                    side.append(ordered[i]);
                return side;
            };
            const QPolygonF top = arc(0, posTR);
            const QPolygonF right = arc(posTR, posBR);
            const QPolygonF bottomArc = arc(posBR, posBL);
            QPolygonF leftArc = arc(posBL, k - 1);
            leftArc.append(ordered[0]); // fecha o passeio no canto TL

            // As curvas do Coons precisam da MESMA direção do eixo u/v
            // (TL→TR e TL→BL); o passeio da base/esquerda é invertido.
            QPolygonF bottom;
            for (int i = bottomArc.size() - 1; i >= 0; --i)
                bottom.append(bottomArc[i]);
            QPolygonF left;
            for (int i = leftArc.size() - 1; i >= 0; --i)
                left.append(leftArc[i]);

            if (top.size() < 2 || right.size() < 2
                || bottom.size() < 2 || left.size() < 2) {
                stretchBox();
            } else {
                // LUTs das curvas parametrizadas pelo comprimento de arco.
                constexpr int kLut = 1024;
                auto buildLut = [](const QPolygonF& poly) {
                    QVector<QPointF> lut(kLut + 1);
                    QVector<double> cum(poly.size(), 0.0);
                    double total = 0.0;
                    for (int i = 1; i < poly.size(); ++i) {
                        total += std::hypot(poly[i].x() - poly[i - 1].x(),
                                            poly[i].y() - poly[i - 1].y());
                        cum[i] = total;
                    }
                    if (total <= 0.0) {
                        for (int i = 0; i <= kLut; ++i)
                            lut[i] = poly[0];
                        return lut;
                    }
                    int seg = 0;
                    for (int i = 0; i <= kLut; ++i) {
                        const double s = (qreal(i) / qreal(kLut)) * total;
                        while (seg < poly.size() - 2 && cum[seg + 1] < s)
                            ++seg;
                        const double t = (s - cum[seg])
                                       / qMax(1e-9, cum[seg + 1] - cum[seg]);
                        lut[i] = poly[seg] + (poly[seg + 1] - poly[seg]) * t;
                    }
                    return lut;
                };
                const QVector<QPointF> topLut = buildLut(top);
                const QVector<QPointF> rightLut = buildLut(right);
                const QVector<QPointF> bottomLut = buildLut(bottom);
                const QVector<QPointF> leftLut = buildLut(left);

                auto curveAt = [](const QVector<QPointF>& lut, double t) {
                    const double s = qBound(0.0, t, 1.0) * qreal(kLut);
                    const int i = qMin(int(s), kLut - 1);
                    const double f = s - qreal(i);
                    return lut[i] + (lut[i + 1] - lut[i]) * f;
                };
                const QPointF cTL = top[0];
                const QPointF cTR = top.last();
                const QPointF cBR = right.last();
                const QPointF cBL = bottom[0];

                auto coonsP = [&](double u, double v) -> QPointF {
                    u = qBound(0.0, u, 1.0);
                    v = qBound(0.0, v, 1.0);
                    const QPointF T = curveAt(topLut, u);
                    const QPointF B = curveAt(bottomLut, u);
                    const QPointF L = curveAt(leftLut, v);
                    const QPointF R = curveAt(rightLut, v);
                    const double w = 1.0 - u;
                    const double hv = 1.0 - v;
                    return T * hv + B * v + L * w + R * u
                         - (cTL * w * hv + cTR * u * hv
                            + cBR * u * v + cBL * w * v);
                };

                // Caixa do patch: chutes iniciais e rejete de pixels fora.
                qreal pMinX = cTL.x(), pMaxX = cTL.x();
                qreal pMinY = cTL.y(), pMaxY = cTL.y();
                for (const QPointF& p : ordered) {
                    pMinX = qMin(pMinX, p.x());
                    pMaxX = qMax(pMaxX, p.x());
                    pMinY = qMin(pMinY, p.y());
                    pMaxY = qMax(pMaxY, p.y());
                }
                if (pMaxX - pMinX < 1.0 || pMaxY - pMinY < 1.0) {
                    result.image = QImage();
                    return result;
                }

                for (int y = 0; y < canvasH; ++y) {
                    QRgb* dstLine =
                        reinterpret_cast<QRgb*>(result.image.scanLine(y));
                    const double yc = qreal(y - pad) + 0.5;
                    if (yc < pMinY || yc > pMaxY)
                        continue;
                    for (int x = 0; x < canvasW; ++x) {
                        const double xc = qreal(x - pad) + 0.5;
                        if (xc < pMinX || xc > pMaxX)
                            continue;
                        const QPointF q(xc, yc);
                        // Chute: fração do bbox. Newton refina rapidamente.
                        double u = qBound(0.0, (xc - pMinX) / (pMaxX - pMinX), 1.0);
                        double v = qBound(0.0, (yc - pMinY) / (pMaxY - pMinY), 1.0);
                        bool ok = false;
                        for (int it = 0; it < 8; ++it) {
                            const QPointF f = coonsP(u, v) - q;
                            if (std::abs(f.x()) + std::abs(f.y()) < 0.3) {
                                ok = true;
                                break;
                            }
                            const double e = 1.0 / 512.0;
                            // Colunas do jacobiano por diferenças finitas
                            // (normalizadas por 2e — sem isso os passos de
                            // Newton saem na escala errada).
                            const double inv2e = 1.0 / (2.0 * e);
                            const QPointF du((coonsP(u + e, v)
                                              - coonsP(u - e, v)) * inv2e);
                            const QPointF dv((coonsP(u, v + e)
                                              - coonsP(u, v - e)) * inv2e);
                            const double det = du.x() * dv.y() - du.y() * dv.x();
                            if (std::abs(det) < 1e-7)
                                break;
                            const double duStep =
                                (f.y() * dv.x() - f.x() * dv.y()) / det;
                            const double dvStep =
                                (f.x() * du.y() - f.y() * du.x()) / det;
                            u = qBound(-0.02, u + duStep, 1.02);
                            v = qBound(-0.02, v + dvStep, 1.02);
                        }
                        if (!ok || u < -0.001 || u > 1.001
                            || v < -0.001 || v > 1.001)
                            continue; // fora do patch: transparente
                        const double sx = qBound(0.0, u, 1.0) * qreal(n - 1);
                        const double sy = qBound(0.0, v, 1.0) * qreal(m - 1);
                        dstLine[x] = sampleBilinear(sx, sy);
                    }
                }
            }
        }
    }

    result.topLeft = QPointF(x0 - pad, y0 - pad);
    return result;
}

} // namespace cc
