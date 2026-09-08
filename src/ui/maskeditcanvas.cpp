#include "maskeditcanvas.h"
#include <QPainter>
#include <QMouseEvent>
#include <QResizeEvent>
#include <algorithm>

namespace cc {

MaskEditCanvas::MaskEditCanvas(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StaticContents, true);
    setMouseTracking(true);
    setFocusPolicy(Qt::ClickFocus);
}

void MaskEditCanvas::setImage(const QImage& original, const QImage& currentMasked, const QColor& bgColor)
{
    m_originalImage = original.convertToFormat(QImage::Format_RGBA8888);
    m_editBuffer = currentMasked.isNull()
        ? m_originalImage.copy()
        : currentMasked.convertToFormat(QImage::Format_RGBA8888);
    m_bgColor = bgColor;
    m_undoBuffer = QImage();
    m_hasUndo = false;
    m_painting = false;
    m_cursorPos = QPoint(-1, -1);
    update();
}

void MaskEditCanvas::setBackgroundColor(const QColor& bgColor)
{
    m_bgColor = bgColor;
    update();
}

void MaskEditCanvas::setMode(Mode mode)
{
    m_mode = mode;
    update();
}

void MaskEditCanvas::setBrushRadius(int radius)
{
    m_brushRadius = std::clamp(radius, 1, 200);
    update();
}

QImage MaskEditCanvas::getEditedImage() const
{
    return m_editBuffer.isNull() ? QImage() : m_editBuffer.copy();
}

void MaskEditCanvas::undo()
{
    if (!m_hasUndo || m_undoBuffer.isNull())
        return;
    m_editBuffer = m_undoBuffer.copy();
    m_hasUndo = false;
    update();
    emit editingDone();
}

void MaskEditCanvas::clearEdits()
{
    if (m_originalImage.isNull())
        return;
    m_editBuffer = m_originalImage.copy();
    m_undoBuffer = QImage();
    m_hasUndo = false;
    m_painting = false;
    m_cursorPos = QPoint(-1, -1);
    update();
}

bool MaskEditCanvas::hasEdits() const
{
    return !m_editBuffer.isNull() && m_editBuffer != m_originalImage;
}

float MaskEditCanvas::scaleFactor() const
{
    if (m_editBuffer.isNull() || width() == 0 || height() == 0)
        return 1.0f;
    const float scaleW = static_cast<float>(width()) / static_cast<float>(m_editBuffer.width());
    const float scaleH = static_cast<float>(height()) / static_cast<float>(m_editBuffer.height());
    return std::min(scaleW, scaleH);
}

QRect MaskEditCanvas::getPixmapRect() const
{
    if (m_editBuffer.isNull())
        return rect();
    const float scale = scaleFactor();
    const int pw = static_cast<int>(m_editBuffer.width() * scale);
    const int ph = static_cast<int>(m_editBuffer.height() * scale);
    const int ox = (width() - pw) / 2;
    const int oy = (height() - ph) / 2;
    return QRect(ox, oy, pw, ph);
}

QPoint MaskEditCanvas::widgetToImage(const QPoint& wgt) const
{
    const QRect pr = getPixmapRect();
    if (!pr.contains(wgt))
        return QPoint(-1, -1);
    const float scale = scaleFactor();
    int imgX = static_cast<int>((wgt.x() - pr.x()) / scale);
    int imgY = static_cast<int>((wgt.y() - pr.y()) / scale);
    imgX = std::clamp(imgX, 0, m_editBuffer.width() - 1);
    imgY = std::clamp(imgY, 0, m_editBuffer.height() - 1);
    return QPoint(imgX, imgY);
}

void MaskEditCanvas::applyBrush(const QPoint& imgPos)
{
    if (m_editBuffer.isNull() || m_originalImage.isNull())
        return;
    const int r = m_brushRadius;
    const int w = m_editBuffer.width();
    const int h = m_editBuffer.height();

    for (int dy = -r; dy <= r; ++dy) {
        for (int dx = -r; dx <= r; ++dx) {
            if (dx * dx + dy * dy > r * r)
                continue;
            const int px = imgPos.x() + dx;
            const int py = imgPos.y() + dy;
            if (px < 0 || px >= w || py < 0 || py >= h)
                continue;

            if (m_mode == Restore) {
                m_editBuffer.setPixel(px, py, m_originalImage.pixel(px, py));
            } else {
                const QRgb pxl = m_editBuffer.pixel(px, py);
                m_editBuffer.setPixel(px, py, qRgba(qRed(pxl), qGreen(pxl), qBlue(pxl), 0));
            }
        }
    }
}

QImage MaskEditCanvas::renderWithBackground(const QImage& img, const QColor& bgColor)
{
    if (img.isNull())
        return QImage();
    QImage result(img.size(), QImage::Format_RGBA8888);

    if (!bgColor.isValid()) {
        for (int y = 0; y < img.height(); ++y) {
            for (int x = 0; x < img.width(); ++x) {
                const bool light = ((x / 10) + (y / 10)) % 2 == 0;
                result.setPixel(x, y, qRgba(
                    light ? 0xDD : 0xBB,
                    light ? 0xDD : 0xBB,
                    light ? 0xDD : 0xBB, 0xFF));
            }
        }
    } else {
        result.fill(bgColor);
    }

    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            const QRgb src = img.pixel(x, y);
            const int alpha = qAlpha(src);
            if (alpha == 255) {
                result.setPixel(x, y, src);
            } else if (alpha > 0) {
                const QRgb dst = result.pixel(x, y);
                const float a = alpha / 255.0f;
                const int r = static_cast<int>(qRed(src) * a + qRed(dst) * (1.0f - a));
                const int g = static_cast<int>(qGreen(src) * a + qGreen(dst) * (1.0f - a));
                const int b = static_cast<int>(qBlue(src) * a + qBlue(dst) * (1.0f - a));
                result.setPixel(x, y, qRgba(r, g, b, 255));
            }
        }
    }
    return result;
}

void MaskEditCanvas::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(25, 27, 30));

    if (m_editBuffer.isNull()) {
        painter.setPen(QColor(180, 180, 180));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("Nenhuma imagem carregada"));
        return;
    }

    const QRect pr = getPixmapRect();
    const QImage composited = renderWithBackground(m_editBuffer, m_bgColor);
    painter.drawImage(pr, composited);

    // Borda ao redor da imagem
    painter.setPen(QColor(70, 75, 85));
    painter.drawRect(pr);

    // Circulo do pincel no cursor
    if (m_cursorPos.x() >= 0 && m_cursorPos.y() >= 0 && pr.contains(m_cursorPos)) {
        const float scale = scaleFactor();
        const float screenR = m_brushRadius * scale;
        QPen brushPen(m_mode == Restore ? QColor(80, 220, 120) : QColor(255, 90, 90), 1.5, Qt::DashLine);
        brushPen.setCosmetic(true);
        painter.setPen(brushPen);
        painter.setBrush(m_mode == Restore ? QColor(80, 220, 120, 30) : QColor(255, 90, 90, 30));
        painter.drawEllipse(QPointF(m_cursorPos), screenR, screenR);
    }
}

void MaskEditCanvas::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_undoBuffer = m_editBuffer.copy();
        m_hasUndo = true;
        m_painting = true;
        const QPoint imgPos = widgetToImage(event->pos());
        if (imgPos.x() >= 0 && imgPos.y() >= 0) {
            applyBrush(imgPos);
            m_lastPoint = imgPos;
            update();
        }
    }
}

void MaskEditCanvas::mouseMoveEvent(QMouseEvent* event)
{
    m_cursorPos = event->pos();
    if (m_painting && (event->buttons() & Qt::LeftButton)) {
        const QPoint imgPos = widgetToImage(event->pos());
        if (imgPos.x() >= 0 && imgPos.y() >= 0) {
            applyBrush(imgPos);
            m_lastPoint = imgPos;
        }
    }
    update();
}

void MaskEditCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_painting) {
        m_painting = false;
        emit editingDone();
        update();
    }
}

void MaskEditCanvas::leaveEvent(QEvent*)
{
    m_cursorPos = QPoint(-1, -1);
    update();
}

} // namespace cc
