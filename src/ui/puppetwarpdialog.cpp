#include "puppetwarpdialog.h"

#include "localization/i18nservice.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

#include <cmath>

namespace cc {

namespace {

struct WarpPin {
    QPointF source;
    QPointF destination;
};

QImage warpImage(const QImage& source, const QVector<WarpPin>& pins)
{
    if (source.isNull() || pins.isEmpty())
        return source;

    QImage result(source.size(), QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    const int width = source.width();
    const int height = source.height();
    constexpr double epsilon = 1e-4;

    for (int y = 0; y < height; ++y) {
        QRgb* destinationLine = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < width; ++x) {
            const QPointF point(x, y);
            QPointF offset;
            double totalWeight = 0.0;
            for (const WarpPin& pin : pins) {
                const QPointF delta = pin.destination - point;
                const double distanceSquared = QPointF::dotProduct(delta, delta);
                // A pin has full influence at its destination and a smooth,
                // diminishing influence elsewhere, like a lightweight puppet warp.
                const double weight = 1.0 / (distanceSquared + 64.0);
                offset += (pin.destination - pin.source) * weight;
                totalWeight += weight;
            }
            if (totalWeight > epsilon)
                offset /= totalWeight;

            const QPointF sample = point - offset;
            const int x0 = static_cast<int>(std::floor(sample.x()));
            const int y0 = static_cast<int>(std::floor(sample.y()));
            if (x0 < 0 || y0 < 0 || x0 + 1 >= width || y0 + 1 >= height)
                continue;
            const double fx = sample.x() - x0;
            const double fy = sample.y() - y0;
            const QColor a = source.pixelColor(x0, y0);
            const QColor b = source.pixelColor(x0 + 1, y0);
            const QColor c = source.pixelColor(x0, y0 + 1);
            const QColor d = source.pixelColor(x0 + 1, y0 + 1);
            const auto interpolate = [fx, fy](int av, int bv, int cv, int dv) {
                const double top = av + (bv - av) * fx;
                const double bottom = cv + (dv - cv) * fx;
                return qBound(0, qRound(top + (bottom - top) * fy), 255);
            };
            destinationLine[x] = qRgba(interpolate(a.red(), b.red(), c.red(), d.red()),
                                       interpolate(a.green(), b.green(), c.green(), d.green()),
                                       interpolate(a.blue(), b.blue(), c.blue(), d.blue()),
                                       interpolate(a.alpha(), b.alpha(), c.alpha(), d.alpha()));
        }
    }
    return result;
}

} // namespace

class PuppetWarpDialog::WarpCanvas final : public QWidget
{
public:
    explicit WarpCanvas(const QImage& source, QWidget* parent = nullptr)
        : QWidget(parent), m_source(source)
    {
        setMinimumSize(520, 360);
        setMouseTracking(true);
        regeneratePreview();
    }

    QImage resultImage() const { return warpImage(m_source, m_pins); }
    void reset()
    {
        m_pins.clear();
        m_activePin = -1;
        regeneratePreview();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(30, 33, 38));
        if (m_preview.isNull())
            return;
        const QRectF target = imageRect();
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.drawImage(target, m_preview);
        painter.setRenderHint(QPainter::Antialiasing, true);
        for (int i = 0; i < m_pins.size(); ++i) {
            const QPointF position = imageToWidget(m_pins.at(i).destination);
            painter.setPen(QPen(QColor(17, 104, 255), 2));
            painter.setBrush(Qt::white);
            painter.drawEllipse(position, 6, 6);
            painter.setPen(Qt::white);
            painter.drawText(position + QPointF(8, -8), QString::number(i + 1));
        }
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() != Qt::LeftButton || !imageRect().contains(event->position()))
            return;
        const QPointF position = widgetToImage(event->position());
        m_activePin = pinAt(event->position());
        if (m_activePin < 0) {
            m_pins.append({position, position});
            m_activePin = m_pins.size() - 1;
        }
        update();
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (m_activePin < 0)
            return;
        m_pins[m_activePin].destination = boundedImagePoint(widgetToImage(event->position()));
        regeneratePreview();
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton)
            m_activePin = -1;
    }

    void resizeEvent(QResizeEvent*) override { regeneratePreview(); }

private:
    QRectF imageRect() const
    {
        if (m_source.isNull())
            return {};
        const QSizeF available = size();
        const QSizeF sourceSize = m_source.size();
        const double scale = qMin(available.width() / sourceSize.width(),
                                  available.height() / sourceSize.height());
        const QSizeF scaled = sourceSize * scale;
        return QRectF((available.width() - scaled.width()) / 2.0,
                      (available.height() - scaled.height()) / 2.0,
                      scaled.width(), scaled.height());
    }

    QPointF imageToWidget(const QPointF& point) const
    {
        const QRectF target = imageRect();
        return {target.left() + point.x() * target.width() / m_source.width(),
                target.top() + point.y() * target.height() / m_source.height()};
    }

    QPointF widgetToImage(const QPointF& point) const
    {
        const QRectF target = imageRect();
        return {(point.x() - target.left()) * m_source.width() / target.width(),
                (point.y() - target.top()) * m_source.height() / target.height()};
    }

    QPointF boundedImagePoint(const QPointF& point) const
    {
        return {qBound(0.0, point.x(), double(m_source.width() - 1)),
                qBound(0.0, point.y(), double(m_source.height() - 1))};
    }

    int pinAt(const QPointF& widgetPoint) const
    {
        for (int i = 0; i < m_pins.size(); ++i) {
            const QPointF delta = imageToWidget(m_pins.at(i).destination) - widgetPoint;
            if (QPointF::dotProduct(delta, delta) <= 14.0 * 14.0)
                return i;
        }
        return -1;
    }

    void regeneratePreview()
    {
        if (m_source.isNull())
            return;
        const QSize maximum(900, 600);
        QImage previewSource = m_source;
        if (previewSource.width() > maximum.width() || previewSource.height() > maximum.height())
            previewSource = previewSource.scaled(maximum, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        QVector<WarpPin> previewPins;
        previewPins.reserve(m_pins.size());
        const double sx = double(previewSource.width()) / m_source.width();
        const double sy = double(previewSource.height()) / m_source.height();
        for (const WarpPin& pin : m_pins)
            previewPins.append({QPointF(pin.source.x() * sx, pin.source.y() * sy),
                                QPointF(pin.destination.x() * sx, pin.destination.y() * sy)});
        m_preview = warpImage(previewSource, previewPins);
        update();
    }

    QImage m_source;
    QImage m_preview;
    QVector<WarpPin> m_pins;
    int m_activePin = -1;
};

PuppetWarpDialog::PuppetWarpDialog(const QImage& source, I18nService* i18n, QWidget* parent)
    : QDialog(parent)
{
    const auto tr = [i18n](const char* key, const QString& fallback) {
        return i18n ? i18n->t("editor", key) : fallback;
    };
    setWindowTitle(tr("puppet.title", QStringLiteral("Puppet Warp")));
    setModal(true);
    resize(760, 620);

    auto* layout = new QVBoxLayout(this);
    auto* hint = new QLabel(tr("puppet.hint", QStringLiteral("Click to add a pin, then drag it to deform the image. Drag an existing pin to adjust it.")), this);
    hint->setWordWrap(true);
    layout->addWidget(hint);
    m_canvas = new WarpCanvas(source, this);
    layout->addWidget(m_canvas, 1);

    auto* bottom = new QHBoxLayout;
    auto* reset = new QPushButton(tr("puppet.reset", QStringLiteral("Reset pins")), this);
    bottom->addWidget(reset);
    bottom->addStretch(1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("puppet.apply", QStringLiteral("Apply")));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("puppet.cancel", QStringLiteral("Cancel")));
    bottom->addWidget(buttons);
    layout->addLayout(bottom);
    connect(reset, &QPushButton::clicked, m_canvas, &WarpCanvas::reset);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QImage PuppetWarpDialog::resultImage() const
{
    return m_canvas ? m_canvas->resultImage() : QImage();
}

} // namespace cc
