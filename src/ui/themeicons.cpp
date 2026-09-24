#include "themeicons.h"

#include <QPainter>
#include <QPainterPath>
#include <QFont>
#include <QLinearGradient>

namespace cc {

namespace {

QPixmap renderPixmap(int size, const std::function<void(QPainter&, int)>& drawFn)
{
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    {
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        drawFn(p, size);
    }
    return pixmap;
}

QIcon makeMultiSizeIcon(const std::function<void(QPainter&, int)>& drawFn)
{
    QIcon icon;
    icon.addPixmap(renderPixmap(24, drawFn));
    icon.addPixmap(renderPixmap(32, drawFn));
    icon.addPixmap(renderPixmap(48, drawFn));
    return icon;
}

} // namespace

QIcon ThemeIcons::toolSelect(const QColor& color)
{
    return makeMultiSizeIcon([color](QPainter& p, int s) {
        p.setPen(QPen(QColor(20, 20, 20), 1.5));
        p.setBrush(color);

        QPainterPath path;
        const qreal scale = s / 24.0;
        path.moveTo(4 * scale, 3 * scale);
        path.lineTo(4 * scale, 19 * scale);
        path.lineTo(8.5 * scale, 15 * scale);
        path.lineTo(12.5 * scale, 21 * scale);
        path.lineTo(14.5 * scale, 19.5 * scale);
        path.lineTo(10.5 * scale, 13.5 * scale);
        path.lineTo(16 * scale, 13.5 * scale);
        path.closeSubpath();
        p.drawPath(path);
    });
}

QIcon ThemeIcons::toolCrop(const QColor& color)
{
    return makeMultiSizeIcon([color](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        QPen pen(color, 2.0 * scale, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);

        // Top-left bracket
        p.drawLine(QPointF(4 * scale, 8 * scale), QPointF(16 * scale, 8 * scale));
        p.drawLine(QPointF(8 * scale, 4 * scale), QPointF(8 * scale, 16 * scale));

        // Bottom-right bracket
        p.drawLine(QPointF(8 * scale, 16 * scale), QPointF(20 * scale, 16 * scale));
        p.drawLine(QPointF(16 * scale, 8 * scale), QPointF(16 * scale, 20 * scale));
    });
}

QIcon ThemeIcons::toolScissors(const QColor& color)
{
    return makeMultiSizeIcon([color](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(color, 1.8 * scale, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);

        // Blades
        p.drawLine(QPointF(6 * scale, 16 * scale), QPointF(19 * scale, 5 * scale));
        p.drawLine(QPointF(6 * scale, 8 * scale), QPointF(19 * scale, 19 * scale));

        // Handles (rings)
        p.drawEllipse(QRectF(3 * scale, 15 * scale, 5 * scale, 5 * scale));
        p.drawEllipse(QRectF(3 * scale, 4 * scale, 5 * scale, 5 * scale));
    });
}

QIcon ThemeIcons::toolWand(const QColor& color)
{
    return makeMultiSizeIcon([color](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        // Wand stick
        p.setPen(QPen(color, 2.0 * scale, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(4 * scale, 20 * scale), QPointF(14 * scale, 10 * scale));

        // Star sparkles
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 215, 0)); // Golden sparkle

        auto drawStar = [&](qreal cx, qreal cy, qreal r) {
            QPainterPath star;
            star.moveTo(cx, cy - r);
            star.quadTo(cx, cy, cx + r, cy);
            star.quadTo(cx, cy, cx, cy + r);
            star.quadTo(cx, cy, cx - r, cy);
            star.quadTo(cx, cy, cx, cy - r);
            p.drawPath(star);
        };

        drawStar(17 * scale, 7 * scale, 4.5 * scale);
        drawStar(11 * scale, 4 * scale, 2.5 * scale);
        drawStar(20 * scale, 13 * scale, 2.2 * scale);
    });
}

QIcon ThemeIcons::toolClone(const QColor& color)
{
    return makeMultiSizeIcon([color](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(color, 1.8 * scale, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(color);

        // Stamp top handle
        p.drawRoundedRect(QRectF(10 * scale, 3 * scale, 4 * scale, 4 * scale), 1 * scale, 1 * scale);
        // Stamp neck
        p.drawRect(QRectF(11 * scale, 7 * scale, 2 * scale, 6 * scale));
        // Stamp body/base
        QPainterPath base;
        base.moveTo(7 * scale, 13 * scale);
        base.lineTo(17 * scale, 13 * scale);
        base.lineTo(19 * scale, 19 * scale);
        base.lineTo(5 * scale, 19 * scale);
        base.closeSubpath();
        p.drawPath(base);
    });
}

QIcon ThemeIcons::toolPaint(const QColor& color)
{
    return makeMultiSizeIcon([color](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        // Handle
        p.setPen(QPen(color, 2.2 * scale, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(19 * scale, 5 * scale), QPointF(11 * scale, 13 * scale));

        // Metal ferrule
        p.setPen(QPen(QColor(180, 180, 180), 3.0 * scale, Qt::SolidLine, Qt::SquareCap));
        p.drawLine(QPointF(11 * scale, 13 * scale), QPointF(9 * scale, 15 * scale));

        // Bristles & tip
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(60, 150, 255));
        QPainterPath tip;
        tip.moveTo(9 * scale, 15 * scale);
        tip.quadTo(5 * scale, 17 * scale, 4 * scale, 20 * scale);
        tip.quadTo(7 * scale, 20 * scale, 9 * scale, 17 * scale);
        tip.closeSubpath();
        p.drawPath(tip);
    });
}

QIcon ThemeIcons::toolFlood(const QColor& color)
{
    return makeMultiSizeIcon([color](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(color, 1.8 * scale, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);

        // Bucket tilted
        QPainterPath bucket;
        bucket.moveTo(6 * scale, 7 * scale);
        bucket.lineTo(14 * scale, 5 * scale);
        bucket.lineTo(18 * scale, 13 * scale);
        bucket.lineTo(10 * scale, 15 * scale);
        bucket.closeSubpath();
        p.drawPath(bucket);

        // Dripping drop
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(60, 150, 255));
        QPainterPath drop;
        drop.moveTo(7 * scale, 15 * scale);
        drop.quadTo(5 * scale, 18 * scale, 5 * scale, 20 * scale);
        drop.quadTo(7 * scale, 21 * scale, 9 * scale, 20 * scale);
        drop.quadTo(9 * scale, 18 * scale, 7 * scale, 15 * scale);
        p.drawPath(drop);
    });
}

QIcon ThemeIcons::toolText(const QColor& color)
{
    return makeMultiSizeIcon([color](QPainter& p, int s) {
        p.setPen(color);
        QFont f = p.font();
        f.setPixelSize(static_cast<int>(s * 0.72));
        f.setBold(true);
        p.setFont(f);
        p.drawText(QRect(0, 0, s, s), Qt::AlignCenter, QStringLiteral("T"));
    });
}

QIcon ThemeIcons::toolShape(const QColor& color)
{
    return makeMultiSizeIcon([color](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(color, 2.0 * scale));
        p.setBrush(QColor(color.red(), color.green(), color.blue(), 60));
        p.drawRoundedRect(QRectF(4 * scale, 4 * scale, 16 * scale, 16 * scale), 2.5 * scale, 2.5 * scale);
    });
}

QIcon ThemeIcons::layerVisibility(bool visible)
{
    return makeMultiSizeIcon([visible](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        QColor color = visible ? QColor(220, 220, 220) : QColor(110, 110, 110);
        p.setPen(QPen(color, 1.8 * scale, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);

        // Eye curve
        QPainterPath eye;
        eye.moveTo(3 * scale, 12 * scale);
        eye.quadTo(12 * scale, 5 * scale, 21 * scale, 12 * scale);
        eye.quadTo(12 * scale, 19 * scale, 3 * scale, 12 * scale);
        p.drawPath(eye);

        // Pupil
        if (visible) {
            p.setBrush(color);
            p.drawEllipse(QPointF(12 * scale, 12 * scale), 3.2 * scale, 3.2 * scale);
        } else {
            // Diagonal slash
            p.setPen(QPen(QColor(220, 80, 80), 2.0 * scale, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(QPointF(4 * scale, 4 * scale), QPointF(20 * scale, 20 * scale));
        }
    });
}

QIcon ThemeIcons::layerLock(bool locked)
{
    return makeMultiSizeIcon([locked](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        QColor color = locked ? QColor(240, 180, 50) : QColor(120, 120, 120);
        p.setPen(QPen(color, 1.8 * scale, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(locked ? color : Qt::NoBrush);

        // Lock body
        p.drawRoundedRect(QRectF(6 * scale, 11 * scale, 12 * scale, 9 * scale), 1.5 * scale, 1.5 * scale);

        // Shackle
        p.setBrush(Qt::NoBrush);
        QPainterPath shackle;
        if (locked) {
            shackle.moveTo(8.5 * scale, 11 * scale);
            shackle.lineTo(8.5 * scale, 7 * scale);
            shackle.arcTo(QRectF(8.5 * scale, 4 * scale, 7 * scale, 6 * scale), 180, -180);
            shackle.lineTo(15.5 * scale, 11 * scale);
        } else {
            // Open shackle
            shackle.moveTo(8.5 * scale, 11 * scale);
            shackle.lineTo(8.5 * scale, 7 * scale);
            shackle.arcTo(QRectF(8.5 * scale, 3 * scale, 7 * scale, 6 * scale), 180, -180);
            shackle.lineTo(15.5 * scale, 6 * scale);
        }
        p.drawPath(shackle);
    });
}

QIcon ThemeIcons::layerTypeText()
{
    return makeMultiSizeIcon([](QPainter& p, int s) {
        p.setPen(QColor(100, 180, 255));
        QFont f = p.font();
        f.setPixelSize(static_cast<int>(s * 0.75));
        f.setBold(true);
        p.setFont(f);
        p.drawText(QRect(0, 0, s, s), Qt::AlignCenter, QStringLiteral("T"));
    });
}

QIcon ThemeIcons::layerTypeImage()
{
    return makeMultiSizeIcon([](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(QColor(120, 210, 120), 1.6 * scale));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(4 * scale, 5 * scale, 16 * scale, 14 * scale), 1.5 * scale, 1.5 * scale);

        // Little sun
        p.setBrush(QColor(120, 210, 120));
        p.drawEllipse(QPointF(8 * scale, 9 * scale), 1.5 * scale, 1.5 * scale);

        // Mountain peak
        p.setBrush(Qt::NoBrush);
        QPainterPath m;
        m.moveTo(5 * scale, 17 * scale);
        m.lineTo(10 * scale, 12 * scale);
        m.lineTo(14 * scale, 16 * scale);
        m.lineTo(17 * scale, 13 * scale);
        m.lineTo(19 * scale, 17 * scale);
        p.drawPath(m);
    });
}

QIcon ThemeIcons::layerTypeShape()
{
    return makeMultiSizeIcon([](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(QColor(230, 150, 70), 1.8 * scale));
        p.setBrush(QColor(230, 150, 70, 80));
        p.drawRect(QRectF(5 * scale, 5 * scale, 14 * scale, 14 * scale));
    });
}

QIcon ThemeIcons::layerTypePaint()
{
    return makeMultiSizeIcon([](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(QColor(220, 120, 220), 2.0 * scale, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(17 * scale, 6 * scale), QPointF(10 * scale, 13 * scale));

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(220, 120, 220));
        QPainterPath tip;
        tip.moveTo(9 * scale, 14 * scale);
        tip.lineTo(6 * scale, 18 * scale);
        tip.lineTo(10 * scale, 17 * scale);
        tip.closeSubpath();
        p.drawPath(tip);
    });
}

QIcon ThemeIcons::layerTypeGroup()
{
    return makeMultiSizeIcon([](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(QColor(200, 200, 200), 1.6 * scale));
        p.setBrush(QColor(200, 200, 200, 40));

        QPainterPath folder;
        folder.moveTo(4 * scale, 7 * scale);
        folder.lineTo(9 * scale, 7 * scale);
        folder.lineTo(11 * scale, 9 * scale);
        folder.lineTo(20 * scale, 9 * scale);
        folder.lineTo(20 * scale, 18 * scale);
        folder.lineTo(4 * scale, 18 * scale);
        folder.closeSubpath();
        p.drawPath(folder);
    });
}

QIcon ThemeIcons::actionAdd()
{
    return makeMultiSizeIcon([](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(QColor(100, 200, 100), 2.2 * scale, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(12 * scale, 5 * scale), QPointF(12 * scale, 19 * scale));
        p.drawLine(QPointF(5 * scale, 12 * scale), QPointF(19 * scale, 12 * scale));
    });
}

QIcon ThemeIcons::actionDuplicate()
{
    return makeMultiSizeIcon([](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(QColor(200, 200, 200), 1.6 * scale));
        p.setBrush(Qt::NoBrush);
        // Back card
        p.drawRoundedRect(QRectF(7 * scale, 4 * scale, 12 * scale, 12 * scale), 1.5 * scale, 1.5 * scale);
        // Front card
        p.setBrush(QColor(50, 50, 50));
        p.drawRoundedRect(QRectF(4 * scale, 7 * scale, 12 * scale, 12 * scale), 1.5 * scale, 1.5 * scale);
    });
}

QIcon ThemeIcons::actionDelete()
{
    return makeMultiSizeIcon([](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(QColor(220, 80, 80), 1.8 * scale, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);

        // Can
        QPainterPath can;
        can.moveTo(6 * scale, 8 * scale);
        can.lineTo(7 * scale, 19 * scale);
        can.lineTo(17 * scale, 19 * scale);
        can.lineTo(18 * scale, 8 * scale);
        p.drawPath(can);

        // Lid
        p.drawLine(QPointF(4 * scale, 8 * scale), QPointF(20 * scale, 8 * scale));
        p.drawLine(QPointF(9 * scale, 5 * scale), QPointF(15 * scale, 5 * scale));
    });
}

QIcon ThemeIcons::actionMoveUp()
{
    return makeMultiSizeIcon([](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(QColor(200, 200, 200), 2.0 * scale, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(6 * scale, 14 * scale), QPointF(12 * scale, 8 * scale));
        p.drawLine(QPointF(12 * scale, 8 * scale), QPointF(18 * scale, 14 * scale));
    });
}

QIcon ThemeIcons::actionMoveDown()
{
    return makeMultiSizeIcon([](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(QColor(200, 200, 200), 2.0 * scale, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(6 * scale, 10 * scale), QPointF(12 * scale, 16 * scale));
        p.drawLine(QPointF(12 * scale, 16 * scale), QPointF(18 * scale, 10 * scale));
    });
}

QIcon ThemeIcons::actionNewDocument()
{
    return makeMultiSizeIcon([](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(QColor(240, 240, 240), 1.8 * scale));
        p.setBrush(Qt::NoBrush);
        // Document outline with folded corner
        QPainterPath doc;
        doc.moveTo(5 * scale, 4 * scale);
        doc.lineTo(14 * scale, 4 * scale);
        doc.lineTo(19 * scale, 9 * scale);
        doc.lineTo(19 * scale, 20 * scale);
        doc.lineTo(5 * scale, 20 * scale);
        doc.closeSubpath();
        p.drawPath(doc);

        // Plus symbol
        p.setPen(QPen(QColor(43, 120, 228), 2.2 * scale, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(12 * scale, 11 * scale), QPointF(12 * scale, 17 * scale));
        p.drawLine(QPointF(9 * scale, 14 * scale), QPointF(15 * scale, 14 * scale));
    });
}

QIcon ThemeIcons::actionOpenFolder()
{
    return makeMultiSizeIcon([](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(QColor(240, 240, 240), 1.8 * scale));
        p.setBrush(Qt::NoBrush);

        QPainterPath f;
        f.moveTo(4 * scale, 6 * scale);
        f.lineTo(10 * scale, 6 * scale);
        f.lineTo(12 * scale, 8 * scale);
        f.lineTo(20 * scale, 8 * scale);
        f.lineTo(20 * scale, 18 * scale);
        f.lineTo(4 * scale, 18 * scale);
        f.closeSubpath();
        p.drawPath(f);
    });
}

QPixmap ThemeIcons::brandYoutube(int size)
{
    return renderPixmap(size, [](QPainter& p, int s) {
        const qreal scale = s / 32.0;
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(230, 33, 23)); // YouTube Red
        p.drawRoundedRect(QRectF(2 * scale, 6 * scale, 28 * scale, 20 * scale), 6 * scale, 6 * scale);

        p.setBrush(Qt::white);
        QPainterPath play;
        play.moveTo(13 * scale, 11 * scale);
        play.lineTo(21 * scale, 16 * scale);
        play.lineTo(13 * scale, 21 * scale);
        play.closeSubpath();
        p.drawPath(play);
    });
}

QPixmap ThemeIcons::brandInstagram(int size)
{
    return renderPixmap(size, [](QPainter& p, int s) {
        const qreal scale = s / 32.0;
        p.setPen(Qt::NoPen);

        QLinearGradient grad(2 * scale, 30 * scale, 30 * scale, 2 * scale);
        grad.setColorAt(0.0, QColor(254, 218, 119));
        grad.setColorAt(0.3, QColor(245, 133, 41));
        grad.setColorAt(0.6, QColor(221, 42, 123));
        grad.setColorAt(1.0, QColor(129, 52, 175));
        p.setBrush(grad);
        p.drawRoundedRect(QRectF(3 * scale, 3 * scale, 26 * scale, 26 * scale), 7 * scale, 7 * scale);

        p.setPen(QPen(Qt::white, 2.0 * scale));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(7 * scale, 7 * scale, 18 * scale, 18 * scale), 4.5 * scale, 4.5 * scale);
        p.drawEllipse(QPointF(16 * scale, 16 * scale), 4.5 * scale, 4.5 * scale);

        p.setBrush(Qt::white);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(21.5 * scale, 10.5 * scale), 1.2 * scale, 1.2 * scale);
    });
}

QPixmap ThemeIcons::brandTiktok(int size)
{
    return renderPixmap(size, [](QPainter& p, int s) {
        const qreal scale = s / 32.0;
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(20, 20, 20));
        p.drawRoundedRect(QRectF(3 * scale, 3 * scale, 26 * scale, 26 * scale), 6 * scale, 6 * scale);

        // Music note shape with cyan/magenta effect
        p.setPen(QPen(QColor(37, 244, 238), 2.4 * scale, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(18 * scale, 8 * scale), QPointF(18 * scale, 20 * scale));
        p.drawLine(QPointF(18 * scale, 8 * scale), QPointF(23 * scale, 11 * scale));

        p.setPen(QPen(QColor(254, 44, 85), 2.4 * scale, Qt::SolidLine, Qt::RoundCap));
        p.drawArc(QRectF(11 * scale, 16 * scale, 7 * scale, 6 * scale), 0, -180 * 16);

        p.setPen(QPen(Qt::white, 2.0 * scale, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(17 * scale, 8 * scale), QPointF(17 * scale, 19 * scale));
    });
}

QPixmap ThemeIcons::brandBanner(int size)
{
    return renderPixmap(size, [](QPainter& p, int s) {
        const qreal scale = s / 32.0;
        p.setPen(Qt::NoPen);
        QLinearGradient grad(2 * scale, 10 * scale, 30 * scale, 22 * scale);
        grad.setColorAt(0.0, QColor(43, 120, 228));
        grad.setColorAt(1.0, QColor(100, 60, 220));
        p.setBrush(grad);
        p.drawRoundedRect(QRectF(2 * scale, 9 * scale, 28 * scale, 14 * scale), 3 * scale, 3 * scale);

        p.setPen(QPen(Qt::white, 1.5 * scale));
        p.drawLine(QPointF(6 * scale, 16 * scale), QPointF(18 * scale, 16 * scale));
    });
}

QPixmap ThemeIcons::brandCustom(int size)
{
    return renderPixmap(size, [](QPainter& p, int s) {
        const qreal scale = s / 32.0;
        p.setPen(QPen(QColor(180, 180, 180), 2.0 * scale, Qt::DashLine));
        p.setBrush(QColor(60, 60, 60, 80));
        p.drawRect(QRectF(5 * scale, 7 * scale, 22 * scale, 18 * scale));

        p.setPen(QPen(QColor(43, 120, 228), 1.8 * scale, Qt::SolidLine));
        // Dimension markers
        p.drawLine(QPointF(3 * scale, 7 * scale), QPointF(3 * scale, 25 * scale));
        p.drawLine(QPointF(5 * scale, 27 * scale), QPointF(27 * scale, 27 * scale));
    });
}

QPixmap ThemeIcons::emptyProjectsPlaceholder(int width, int height)
{
    QPixmap pixmap(width, height);
    pixmap.fill(Qt::transparent);
    {
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing, true);

        const qreal scale = qMin(width, height) / 64.0;
        const qreal cx = width / 2.0;
        const qreal cy = height / 2.0;

        // Subtle glowing background circle
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(50, 55, 68, 120));
        p.drawEllipse(QPointF(cx, cy), 28 * scale, 28 * scale);

        // Canvas board outline
        p.setPen(QPen(QColor(130, 140, 160), 2.0 * scale));
        p.setBrush(QColor(40, 44, 54));
        p.drawRoundedRect(QRectF(cx - 16 * scale, cy - 18 * scale, 32 * scale, 36 * scale), 3 * scale, 3 * scale);

        // Subtle mountain / landscape inside empty canvas
        p.setPen(QPen(QColor(90, 100, 120), 1.6 * scale));
        QPainterPath peak;
        peak.moveTo(cx - 12 * scale, cy + 10 * scale);
        peak.lineTo(cx - 3 * scale, cy);
        peak.lineTo(cx + 4 * scale, cy + 6 * scale);
        peak.lineTo(cx + 12 * scale, cy - 4 * scale);
        peak.lineTo(cx + 12 * scale, cy + 10 * scale);
        p.drawPath(peak);

        // Sparkle / plus near top corner
        p.setPen(QPen(QColor(43, 120, 228), 2.2 * scale, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(cx + 10 * scale, cy - 14 * scale), QPointF(cx + 16 * scale, cy - 14 * scale));
        p.drawLine(QPointF(cx + 13 * scale, cy - 17 * scale), QPointF(cx + 13 * scale, cy - 11 * scale));
    }
    return pixmap;
}

QIcon ThemeIcons::appIcon()
{
    auto drawAppLogo = [](QPainter& p, int s) {
        const qreal scale = s / 64.0;
        // Background rounded rectangle with linear gradient
        QLinearGradient g(0, 0, 0, 64 * scale);
        g.setColorAt(0.0, QColor(0x2f, 0x6f, 0xed));
        g.setColorAt(1.0, QColor(0x0d, 0x3a, 0x99));
        p.setPen(Qt::NoPen);
        p.setBrush(g);
        p.drawRoundedRect(QRectF(4 * scale, 4 * scale, 56 * scale, 56 * scale), 12 * scale, 12 * scale);

        // Canvas / document sheet
        p.setBrush(QColor(255, 255, 255, 245));
        p.drawRoundedRect(QRectF(12 * scale, 14 * scale, 40 * scale, 36 * scale), 4 * scale, 4 * scale);

        // Header bar
        p.setBrush(QColor(0x1a, 0x1b, 0x1f));
        QPainterPath headerPath;
        headerPath.addRoundedRect(QRectF(12 * scale, 14 * scale, 40 * scale, 9 * scale), 4 * scale, 4 * scale);
        p.drawPath(headerPath);
        p.drawRect(QRectF(12 * scale, 19 * scale, 40 * scale, 4 * scale));

        // Window controls dots
        p.setBrush(QColor(0xff, 0x60, 0x58));
        p.drawEllipse(QPointF(17 * scale, 18.5 * scale), 1.8 * scale, 1.8 * scale);
        p.setBrush(QColor(0xff, 0xbd, 0x2e));
        p.drawEllipse(QPointF(22 * scale, 18.5 * scale), 1.8 * scale, 1.8 * scale);
        p.setBrush(QColor(0x28, 0xc9, 0x40));
        p.drawEllipse(QPointF(27 * scale, 18.5 * scale), 1.8 * scale, 1.8 * scale);

        // Content / layout lines
        p.setPen(QPen(QColor(0x1a, 0x1b, 0x1f), 2.5 * scale, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(20 * scale, 31 * scale), QPointF(44 * scale, 31 * scale));
        p.drawLine(QPointF(20 * scale, 37 * scale), QPointF(44 * scale, 37 * scale));
        p.drawLine(QPointF(20 * scale, 43 * scale), QPointF(34 * scale, 43 * scale));
    };

    QIcon icon;
    icon.addPixmap(renderPixmap(16, drawAppLogo));
    icon.addPixmap(renderPixmap(24, drawAppLogo));
    icon.addPixmap(renderPixmap(32, drawAppLogo));
    icon.addPixmap(renderPixmap(48, drawAppLogo));
    icon.addPixmap(renderPixmap(64, drawAppLogo));
    icon.addPixmap(renderPixmap(128, drawAppLogo));
    icon.addPixmap(renderPixmap(256, drawAppLogo));
    return icon;
}

QIcon ThemeIcons::actionSave(const QColor& color)
{
    return makeMultiSizeIcon([color](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(color, 1.8 * scale, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);

        // Floppy outline with cut corner
        QPainterPath path;
        path.moveTo(4 * scale, 4 * scale);
        path.lineTo(17 * scale, 4 * scale);
        path.lineTo(20 * scale, 7 * scale);
        path.lineTo(20 * scale, 20 * scale);
        path.lineTo(4 * scale, 20 * scale);
        path.closeSubpath();
        p.drawPath(path);

        // Top shutter
        p.fillRect(QRectF(8 * scale, 5 * scale, 8 * scale, 5 * scale), color);
        // Slider hole
        p.fillRect(QRectF(10 * scale, 6 * scale, 2 * scale, 3 * scale), QColor(30, 34, 42));

        // Bottom label
        p.drawRoundedRect(QRectF(7 * scale, 13 * scale, 10 * scale, 7 * scale), 1 * scale, 1 * scale);
    });
}

QIcon ThemeIcons::actionSaveAs(const QColor& color)
{
    return makeMultiSizeIcon([color](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(color, 1.8 * scale, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);

        // Floppy outline
        QPainterPath path;
        path.moveTo(3 * scale, 3 * scale);
        path.lineTo(15 * scale, 3 * scale);
        path.lineTo(18 * scale, 6 * scale);
        path.lineTo(18 * scale, 15 * scale);
        path.lineTo(3 * scale, 15 * scale);
        path.closeSubpath();
        p.drawPath(path);

        // Pen/Pencil in bottom right
        p.setPen(QPen(QColor(43, 120, 228), 2.0 * scale, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(14 * scale, 21 * scale), QPointF(21 * scale, 14 * scale));
        p.setPen(QPen(color, 1.5 * scale, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(12 * scale, 22 * scale), QPointF(14 * scale, 21 * scale));
    });
}

QIcon ThemeIcons::actionImport(const QColor& color)
{
    return makeMultiSizeIcon([color](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(color, 1.8 * scale, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);

        // Picture frame
        p.drawRoundedRect(QRectF(4 * scale, 4 * scale, 16 * scale, 16 * scale), 2 * scale, 2 * scale);

        // Downward arrow
        p.setPen(QPen(QColor(43, 120, 228), 2.2 * scale, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(12 * scale, 8 * scale), QPointF(12 * scale, 15 * scale));
        p.drawLine(QPointF(9 * scale, 12 * scale), QPointF(12 * scale, 15 * scale));
        p.drawLine(QPointF(15 * scale, 12 * scale), QPointF(12 * scale, 15 * scale));
    });
}

QIcon ThemeIcons::actionExport(const QColor& color)
{
    return makeMultiSizeIcon([color](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(color, 1.8 * scale, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);

        // Open tray
        QPainterPath tray;
        tray.moveTo(4 * scale, 12 * scale);
        tray.lineTo(4 * scale, 20 * scale);
        tray.lineTo(20 * scale, 20 * scale);
        tray.lineTo(20 * scale, 12 * scale);
        p.drawPath(tray);

        // Upward arrow
        p.setPen(QPen(QColor(43, 120, 228), 2.2 * scale, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(12 * scale, 15 * scale), QPointF(12 * scale, 4 * scale));
        p.drawLine(QPointF(8 * scale, 8 * scale), QPointF(12 * scale, 4 * scale));
        p.drawLine(QPointF(16 * scale, 8 * scale), QPointF(12 * scale, 4 * scale));
    });
}

QIcon ThemeIcons::actionQuit(const QColor& color)
{
    return makeMultiSizeIcon([color](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(color, 2.0 * scale, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);

        // Power arc
        p.drawArc(QRectF(4 * scale, 4 * scale, 16 * scale, 16 * scale), 45 * 16, 270 * 16);
        // Vertical power line
        p.setPen(QPen(QColor(240, 80, 80), 2.0 * scale, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(12 * scale, 3 * scale), QPointF(12 * scale, 11 * scale));
    });
}

QIcon ThemeIcons::actionUndo(const QColor& color)
{
    return makeMultiSizeIcon([color](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(color, 2.0 * scale, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);

        // Arc curving left
        QPainterPath path;
        path.moveTo(18 * scale, 19 * scale);
        path.cubicTo(18 * scale, 10 * scale, 8 * scale, 10 * scale, 7 * scale, 13 * scale);
        p.drawPath(path);

        // Arrow head pointing left
        p.drawLine(QPointF(11 * scale, 9 * scale), QPointF(6 * scale, 13 * scale));
        p.drawLine(QPointF(11 * scale, 17 * scale), QPointF(6 * scale, 13 * scale));
    });
}

QIcon ThemeIcons::actionRedo(const QColor& color)
{
    return makeMultiSizeIcon([color](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(color, 2.0 * scale, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);

        // Arc curving right
        QPainterPath path;
        path.moveTo(6 * scale, 19 * scale);
        path.cubicTo(6 * scale, 10 * scale, 16 * scale, 10 * scale, 17 * scale, 13 * scale);
        p.drawPath(path);

        // Arrow head pointing right
        p.drawLine(QPointF(13 * scale, 9 * scale), QPointF(18 * scale, 13 * scale));
        p.drawLine(QPointF(13 * scale, 17 * scale), QPointF(18 * scale, 13 * scale));
    });
}

QIcon ThemeIcons::actionHistory(const QColor& color)
{
    return makeMultiSizeIcon([color](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(color, 1.8 * scale, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);

        // Circular clock outline with arrow at top-left
        QRectF clockRect(4 * scale, 4 * scale, 16 * scale, 16 * scale);
        p.drawArc(clockRect, 45 * 16, 270 * 16);

        // Arrow head pointing counter-clockwise
        p.drawLine(QPointF(9 * scale, 2 * scale), QPointF(12 * scale, 5 * scale));
        p.drawLine(QPointF(9 * scale, 8 * scale), QPointF(12 * scale, 5 * scale));

        // Clock hands (center 12, 12)
        p.drawLine(QPointF(12 * scale, 12 * scale), QPointF(12 * scale, 8 * scale));
        p.drawLine(QPointF(12 * scale, 12 * scale), QPointF(15 * scale, 12 * scale));
    });
}

QIcon ThemeIcons::actionZoomIn(const QColor& color)
{
    return makeMultiSizeIcon([color](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(color, 2.0 * scale, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);

        // Glass circle
        p.drawEllipse(QRectF(4 * scale, 4 * scale, 12 * scale, 12 * scale));
        // Handle
        p.drawLine(QPointF(14.5 * scale, 14.5 * scale), QPointF(20 * scale, 20 * scale));

        // Plus
        p.drawLine(QPointF(7 * scale, 10 * scale), QPointF(13 * scale, 10 * scale));
        p.drawLine(QPointF(10 * scale, 7 * scale), QPointF(10 * scale, 13 * scale));
    });
}

QIcon ThemeIcons::actionZoomOut(const QColor& color)
{
    return makeMultiSizeIcon([color](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(color, 2.0 * scale, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);

        // Glass circle
        p.drawEllipse(QRectF(4 * scale, 4 * scale, 12 * scale, 12 * scale));
        // Handle
        p.drawLine(QPointF(14.5 * scale, 14.5 * scale), QPointF(20 * scale, 20 * scale));

        // Minus
        p.drawLine(QPointF(7 * scale, 10 * scale), QPointF(13 * scale, 10 * scale));
    });
}

QIcon ThemeIcons::actionZoomFit(const QColor& color)
{
    return makeMultiSizeIcon([color](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(color, 2.0 * scale, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);

        // 4 corner brackets
        // Top-left
        p.drawLine(QPointF(4 * scale, 9 * scale), QPointF(4 * scale, 4 * scale));
        p.drawLine(QPointF(4 * scale, 4 * scale), QPointF(9 * scale, 4 * scale));

        // Top-right
        p.drawLine(QPointF(15 * scale, 4 * scale), QPointF(20 * scale, 4 * scale));
        p.drawLine(QPointF(20 * scale, 4 * scale), QPointF(20 * scale, 9 * scale));

        // Bottom-left
        p.drawLine(QPointF(4 * scale, 15 * scale), QPointF(4 * scale, 20 * scale));
        p.drawLine(QPointF(4 * scale, 20 * scale), QPointF(9 * scale, 20 * scale));

        // Bottom-right
        p.drawLine(QPointF(15 * scale, 20 * scale), QPointF(20 * scale, 20 * scale));
        p.drawLine(QPointF(20 * scale, 20 * scale), QPointF(20 * scale, 15 * scale));

        // Center dot
        p.setBrush(color);
        p.drawEllipse(QRectF(10.5 * scale, 10.5 * scale, 3 * scale, 3 * scale));
    });
}

QIcon ThemeIcons::actionGrid(const QColor& color)
{
    return makeMultiSizeIcon([color](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(color, 1.6 * scale, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);

        p.drawRoundedRect(QRectF(4 * scale, 4 * scale, 16 * scale, 16 * scale), 2 * scale, 2 * scale);
        // Vertical grid lines
        p.drawLine(QPointF(9.3 * scale, 4 * scale), QPointF(9.3 * scale, 20 * scale));
        p.drawLine(QPointF(14.7 * scale, 4 * scale), QPointF(14.7 * scale, 20 * scale));
        // Horizontal grid lines
        p.drawLine(QPointF(4 * scale, 9.3 * scale), QPointF(20 * scale, 9.3 * scale));
        p.drawLine(QPointF(4 * scale, 14.7 * scale), QPointF(20 * scale, 14.7 * scale));
    });
}

QIcon ThemeIcons::actionSnap(const QColor& color)
{
    return makeMultiSizeIcon([color](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(color, 2.2 * scale, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);

        // Horseshoe magnet
        QPainterPath magnet;
        magnet.moveTo(6 * scale, 7 * scale);
        magnet.lineTo(6 * scale, 14 * scale);
        magnet.arcTo(QRectF(6 * scale, 8 * scale, 12 * scale, 12 * scale), 180, -180);
        magnet.lineTo(18 * scale, 7 * scale);
        p.drawPath(magnet);

        // North pole cap
        p.fillRect(QRectF(4.5 * scale, 4 * scale, 3.5 * scale, 3.5 * scale), QColor(240, 70, 70));
        // South pole cap
        p.fillRect(QRectF(16 * scale, 4 * scale, 3.5 * scale, 3.5 * scale), QColor(50, 130, 240));
    });
}

QIcon ThemeIcons::actionFlipH(const QColor& color)
{
    return makeMultiSizeIcon([color](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(QColor(120, 130, 150), 1.5 * scale, Qt::DashLine));
        p.drawLine(QPointF(12 * scale, 3 * scale), QPointF(12 * scale, 21 * scale));

        p.setPen(QPen(color, 1.8 * scale));
        // Left triangle
        QPolygonF t1;
        t1 << QPointF(10 * scale, 6 * scale) << QPointF(3 * scale, 12 * scale) << QPointF(10 * scale, 18 * scale);
        p.setBrush(QColor(color.red(), color.green(), color.blue(), 100));
        p.drawPolygon(t1);

        // Right triangle
        QPolygonF t2;
        t2 << QPointF(14 * scale, 6 * scale) << QPointF(21 * scale, 12 * scale) << QPointF(14 * scale, 18 * scale);
        p.setBrush(Qt::NoBrush);
        p.drawPolygon(t2);
    });
}

QIcon ThemeIcons::actionFlipV(const QColor& color)
{
    return makeMultiSizeIcon([color](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(QColor(120, 130, 150), 1.5 * scale, Qt::DashLine));
        p.drawLine(QPointF(3 * scale, 12 * scale), QPointF(21 * scale, 12 * scale));

        p.setPen(QPen(color, 1.8 * scale));
        // Top triangle
        QPolygonF t1;
        t1 << QPointF(6 * scale, 10 * scale) << QPointF(12 * scale, 3 * scale) << QPointF(18 * scale, 10 * scale);
        p.setBrush(QColor(color.red(), color.green(), color.blue(), 100));
        p.drawPolygon(t1);

        // Bottom triangle
        QPolygonF t2;
        t2 << QPointF(6 * scale, 14 * scale) << QPointF(12 * scale, 21 * scale) << QPointF(18 * scale, 14 * scale);
        p.setBrush(Qt::NoBrush);
        p.drawPolygon(t2);
    });
}

QIcon ThemeIcons::actionAlignLeft(const QColor& color)
{
    return makeMultiSizeIcon([color](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(QColor(43, 120, 228), 2.2 * scale, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(4 * scale, 3 * scale), QPointF(4 * scale, 21 * scale));

        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawRoundedRect(QRectF(7 * scale, 6 * scale, 13 * scale, 4 * scale), 1 * scale, 1 * scale);
        p.drawRoundedRect(QRectF(7 * scale, 14 * scale, 8 * scale, 4 * scale), 1 * scale, 1 * scale);
    });
}

QIcon ThemeIcons::actionAlignCenter(const QColor& color)
{
    return makeMultiSizeIcon([color](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(QColor(43, 120, 228), 2.2 * scale, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(12 * scale, 3 * scale), QPointF(12 * scale, 21 * scale));

        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawRoundedRect(QRectF(5 * scale, 6 * scale, 14 * scale, 4 * scale), 1 * scale, 1 * scale);
        p.drawRoundedRect(QRectF(8 * scale, 14 * scale, 8 * scale, 4 * scale), 1 * scale, 1 * scale);
    });
}

QIcon ThemeIcons::actionAlignRight(const QColor& color)
{
    return makeMultiSizeIcon([color](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(QColor(43, 120, 228), 2.2 * scale, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(20 * scale, 3 * scale), QPointF(20 * scale, 21 * scale));

        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawRoundedRect(QRectF(4 * scale, 6 * scale, 13 * scale, 4 * scale), 1 * scale, 1 * scale);
        p.drawRoundedRect(QRectF(9 * scale, 14 * scale, 8 * scale, 4 * scale), 1 * scale, 1 * scale);
    });
}

QIcon ThemeIcons::actionAlignTop(const QColor& color)
{
    return makeMultiSizeIcon([color](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(QColor(43, 120, 228), 2.2 * scale, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(3 * scale, 4 * scale), QPointF(21 * scale, 4 * scale));

        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawRoundedRect(QRectF(6 * scale, 7 * scale, 4 * scale, 13 * scale), 1 * scale, 1 * scale);
        p.drawRoundedRect(QRectF(14 * scale, 7 * scale, 4 * scale, 8 * scale), 1 * scale, 1 * scale);
    });
}

QIcon ThemeIcons::actionAlignMiddle(const QColor& color)
{
    return makeMultiSizeIcon([color](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(QColor(43, 120, 228), 2.2 * scale, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(3 * scale, 12 * scale), QPointF(21 * scale, 12 * scale));

        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawRoundedRect(QRectF(6 * scale, 5 * scale, 4 * scale, 14 * scale), 1 * scale, 1 * scale);
        p.drawRoundedRect(QRectF(14 * scale, 8 * scale, 4 * scale, 8 * scale), 1 * scale, 1 * scale);
    });
}

QIcon ThemeIcons::actionAlignBottom(const QColor& color)
{
    return makeMultiSizeIcon([color](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(QColor(43, 120, 228), 2.2 * scale, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(3 * scale, 20 * scale), QPointF(21 * scale, 20 * scale));

        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawRoundedRect(QRectF(6 * scale, 4 * scale, 4 * scale, 13 * scale), 1 * scale, 1 * scale);
        p.drawRoundedRect(QRectF(14 * scale, 9 * scale, 4 * scale, 8 * scale), 1 * scale, 1 * scale);
    });
}

QIcon ThemeIcons::actionDistributeH(const QColor& color)
{
    return makeMultiSizeIcon([color](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(QColor(43, 120, 228), 1.8 * scale, Qt::SolidLine));
        p.drawLine(QPointF(3 * scale, 4 * scale), QPointF(3 * scale, 20 * scale));
        p.drawLine(QPointF(21 * scale, 4 * scale), QPointF(21 * scale, 20 * scale));

        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawRoundedRect(QRectF(8 * scale, 6 * scale, 3 * scale, 12 * scale), 1 * scale, 1 * scale);
        p.drawRoundedRect(QRectF(14 * scale, 6 * scale, 3 * scale, 12 * scale), 1 * scale, 1 * scale);
    });
}

QIcon ThemeIcons::actionDistributeV(const QColor& color)
{
    return makeMultiSizeIcon([color](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(QColor(43, 120, 228), 1.8 * scale, Qt::SolidLine));
        p.drawLine(QPointF(4 * scale, 3 * scale), QPointF(20 * scale, 3 * scale));
        p.drawLine(QPointF(4 * scale, 21 * scale), QPointF(20 * scale, 21 * scale));

        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawRoundedRect(QRectF(6 * scale, 8 * scale, 12 * scale, 3 * scale), 1 * scale, 1 * scale);
        p.drawRoundedRect(QRectF(6 * scale, 14 * scale, 12 * scale, 3 * scale), 1 * scale, 1 * scale);
    });
}

QIcon ThemeIcons::actionSettings(const QColor& color)
{
    return makeMultiSizeIcon([color](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(color, 2.0 * scale, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);

        p.drawEllipse(QPointF(12 * scale, 12 * scale), 4.5 * scale, 4.5 * scale);

        // 6 gear cogs
        for (int i = 0; i < 6; ++i) {
            const double angle = i * (M_PI / 3.0);
            const qreal x1 = 12 * scale + std::cos(angle) * (6.5 * scale);
            const qreal y1 = 12 * scale + std::sin(angle) * (6.5 * scale);
            const qreal x2 = 12 * scale + std::cos(angle) * (9.5 * scale);
            const qreal y2 = 12 * scale + std::sin(angle) * (9.5 * scale);
            p.drawLine(QPointF(x1, y1), QPointF(x2, y2));
        }
    });
}

QIcon ThemeIcons::actionAbout(const QColor& color)
{
    return makeMultiSizeIcon([color](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(color, 1.8 * scale, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);

        p.drawEllipse(QRectF(4 * scale, 4 * scale, 16 * scale, 16 * scale));

        // 'i' dot
        p.setBrush(color);
        p.drawEllipse(QRectF(11 * scale, 7.5 * scale, 2 * scale, 2 * scale));

        // 'i' stem
        p.drawLine(QPointF(12 * scale, 11 * scale), QPointF(12 * scale, 16 * scale));
    });
}

QIcon ThemeIcons::actionCheck(const QColor& color)
{
    return makeMultiSizeIcon([color](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(color, 2.4 * scale, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);

        QPainterPath check;
        check.moveTo(5 * scale, 12 * scale);
        check.lineTo(10 * scale, 17 * scale);
        check.lineTo(19 * scale, 7 * scale);
        p.drawPath(check);
    });
}

QIcon ThemeIcons::actionCancel(const QColor& color)
{
    return makeMultiSizeIcon([color](QPainter& p, int s) {
        const qreal scale = s / 24.0;
        p.setPen(QPen(color, 2.2 * scale, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);

        p.drawLine(QPointF(6 * scale, 6 * scale), QPointF(18 * scale, 18 * scale));
        p.drawLine(QPointF(18 * scale, 6 * scale), QPointF(6 * scale, 18 * scale));
    });
}

} // namespace cc
