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

} // namespace cc
