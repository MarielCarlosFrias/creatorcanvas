#include "ImageProcessing.h"

#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>
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
        QPen pen(Qt::transparent, clampedSize, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
        painter.drawLine(prevPoint, curPoint);
        break;
    }
    case BrushType::Pencil: {
        painter.setRenderHint(QPainter::Antialiasing, false);
        QColor c = color;
        c.setAlphaF(clampedOpacity);
        QPen pen(c, clampedSize, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin);
        painter.setPen(pen);
        painter.drawLine(prevPoint, curPoint);
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
        QPen pen(c, clampedSize, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
        painter.drawLine(prevPoint, curPoint);
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

    std::vector<bool> visited(w * h, false);
    std::vector<QPoint> queue;
    queue.reserve(w * 8);
    queue.push_back(seedPoint);
    visited[seedPoint.y() * w + seedPoint.x()] = true;

    auto colorMatch = [&](QRgb px) -> bool {
        const double d = std::sqrt(
            std::pow(qRed(px) - tr, 2) +
            std::pow(qGreen(px) - tg, 2) +
            std::pow(qBlue(px) - tb, 2) +
            std::pow(qAlpha(px) - ta, 2));
        return d <= tolDistance;
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
                                    double saturation, double temperature)
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

    for (int y = 0; y < h; ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < w; ++x) {
            const QRgb px = line[x];
            const int a = qAlpha(px);
            if (a == 0) continue;

            double r = qRed(px);
            double g = qGreen(px);
            double b = qBlue(px);

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

            line[x] = qRgba(std::clamp(static_cast<int>(std::round(r)), 0, 255),
                            std::clamp(static_cast<int>(std::round(g)), 0, 255),
                            std::clamp(static_cast<int>(std::round(b)), 0, 255),
                            a);
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
            }
        }
    }

    return result;
}

} // namespace cc
