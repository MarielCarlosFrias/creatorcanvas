#include "transformselectiondialog.h"

#include "localization/i18nservice.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace cc {
namespace {

QDoubleSpinBox* makeSpinBox(QWidget* parent, const char* objectName,
                            double minimum, double maximum, double value,
                            int decimals, double step, const QString& suffix)
{
    auto* spin = new QDoubleSpinBox(parent);
    spin->setObjectName(QLatin1String(objectName));
    spin->setRange(minimum, maximum);
    spin->setDecimals(decimals);
    spin->setSingleStep(step);
    spin->setValue(value);
    spin->setSuffix(suffix);
    spin->setKeyboardTracking(false);
    return spin;
}

QImage previewImage(const QImage& original, const QImage& transformed,
                    const QSize& maximumSize)
{
    if (original.isNull())
        return {};

    QSize size = original.size();
    size.scale(maximumSize, Qt::KeepAspectRatio);
    size.setWidth(std::max(1, size.width()));
    size.setHeight(std::max(1, size.height()));

    const QImage before = original.convertToFormat(QImage::Format_Alpha8)
                              .scaled(size, Qt::IgnoreAspectRatio,
                                      Qt::SmoothTransformation)
                              .convertToFormat(QImage::Format_Alpha8);
    QImage after;
    if (!transformed.isNull()) {
        after = transformed.convertToFormat(QImage::Format_Alpha8)
                    .scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                    .convertToFormat(QImage::Format_Alpha8);
    }

    QImage result(size, QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < size.height(); ++y) {
        const uchar* oldRow = before.constScanLine(y);
        const uchar* newRow = after.isNull() ? nullptr : after.constScanLine(y);
        QRgb* output = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < size.width(); ++x) {
            // Original: cinza discreto. Resultado: azul opaco por cobertura.
            const int oldCoverage = oldRow[x];
            const int base = 24 + oldCoverage * 42 / 255;
            const int newCoverage = newRow ? newRow[x] : 0;
            const auto blend = [newCoverage](int foreground, int background) {
                return (foreground * newCoverage + background * (255 - newCoverage)) / 255;
            };
            output[x] = qRgba(blend(56, base), blend(189, base), blend(248, base), 255);
        }
    }
    return result;
}

} // namespace

TransformSelectionDialog::TransformSelectionDialog(const QImage& currentSelection,
                                                   I18nService* i18n, QWidget* parent)
    : QDialog(parent)
    , m_original(currentSelection.isNull()
                     ? QImage()
                     : currentSelection.convertToFormat(QImage::Format_Alpha8))
    , m_i18n(i18n)
{
    setWindowTitle(text(QStringLiteral("title"), QStringLiteral("Transform Selection")));
    setMinimumSize(400, 470);

    auto* layout = new QVBoxLayout(this);
    auto* explanation = new QLabel(
        text(QStringLiteral("hint"),
             QStringLiteral("Move, scale, or rotate only the selection outline. Layer pixels are not changed.")),
        this);
    explanation->setWordWrap(true);
    layout->addWidget(explanation);

    auto* form = new QFormLayout;
    const double moveLimit = std::max(10000.0,
        4.0 * std::max(m_original.width(), m_original.height()));
    const QString pixels = QStringLiteral(" px");
    m_translateX = makeSpinBox(this, "translateX", -moveLimit, moveLimit,
                               0.0, 2, 1.0, pixels);
    m_translateY = makeSpinBox(this, "translateY", -moveLimit, moveLimit,
                               0.0, 2, 1.0, pixels);
    m_scaleX = makeSpinBox(this, "scaleX", 0.1, 1000.0, 100.0,
                           2, 10.0, QStringLiteral("%"));
    m_scaleY = makeSpinBox(this, "scaleY", 0.1, 1000.0, 100.0,
                           2, 10.0, QStringLiteral("%"));
    m_rotation = makeSpinBox(this, "rotation", -3600.0, 3600.0, 0.0,
                             2, 1.0, QStringLiteral("°"));

    auto* moveRow = new QHBoxLayout;
    moveRow->addWidget(m_translateX);
    moveRow->addWidget(m_translateY);
    form->addRow(text(QStringLiteral("move"), QStringLiteral("Move X / Y")), moveRow);

    auto* scaleRow = new QHBoxLayout;
    scaleRow->addWidget(m_scaleX);
    scaleRow->addWidget(m_scaleY);
    form->addRow(text(QStringLiteral("scale"), QStringLiteral("Scale W / H")), scaleRow);
    form->addRow(text(QStringLiteral("rotation"), QStringLiteral("Rotation")), m_rotation);
    layout->addLayout(form);

    m_preview = new QLabel(this);
    m_preview->setObjectName(QStringLiteral("preview"));
    m_preview->setMinimumSize(320, 210);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setStyleSheet(QStringLiteral(
        "background-color: #181a1f; border: 1px solid #2b303c; border-radius: 6px;"));
    layout->addWidget(m_preview, 1);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel
                                         | QDialogButtonBox::Reset,
                                     this);
    m_buttons->button(QDialogButtonBox::Ok)->setText(
        text(QStringLiteral("apply"), QStringLiteral("Apply")));
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(!m_original.isNull());
    m_buttons->button(QDialogButtonBox::Reset)->setText(
        text(QStringLiteral("reset"), QStringLiteral("Reset")));
    layout->addWidget(m_buttons);

    m_previewTimer = new QTimer(this);
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(60);
    connect(m_previewTimer, &QTimer::timeout,
            this, [this]() { updatePreview(); });

    const auto schedulePreview = [this](double) { m_previewTimer->start(); };
    connect(m_translateX, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, schedulePreview);
    connect(m_translateY, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, schedulePreview);
    connect(m_scaleX, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, schedulePreview);
    connect(m_scaleY, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, schedulePreview);
    connect(m_rotation, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, schedulePreview);

    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_buttons->button(QDialogButtonBox::Reset), &QPushButton::clicked,
            this, [this]() { resetParameters(); });

    updatePreview();
}

SelectionTransformParameters TransformSelectionDialog::parameters() const
{
    SelectionTransformParameters result;
    if (m_translateX)
        result.translation = QPointF(m_translateX->value(), m_translateY->value());
    if (m_scaleX) {
        result.scaleXPercent = m_scaleX->value();
        result.scaleYPercent = m_scaleY->value();
    }
    if (m_rotation)
        result.rotationDegrees = m_rotation->value();
    return result;
}

QImage TransformSelectionDialog::transformedSelection() const
{
    return SelectionTransform::apply(m_original, parameters());
}

QString TransformSelectionDialog::text(const QString& key, const QString& fallback) const
{
    if (!m_i18n)
        return fallback;
    const QString fullKey = QStringLiteral("selection.transform.") + key;
    const QString translated = m_i18n->t(QStringLiteral("editor"), fullKey);
    return translated.isEmpty() || translated == fullKey ? fallback : translated;
}

void TransformSelectionDialog::resetParameters()
{
    m_previewTimer->stop();
    m_translateX->setValue(0.0);
    m_translateY->setValue(0.0);
    m_scaleX->setValue(100.0);
    m_scaleY->setValue(100.0);
    m_rotation->setValue(0.0);
    updatePreview();
}

void TransformSelectionDialog::updatePreview()
{
    if (!m_preview)
        return;
    if (m_original.isNull()) {
        m_preview->setPixmap(QPixmap());
        m_preview->setText(text(QStringLiteral("empty"), QStringLiteral("No active selection")));
        return;
    }

    m_preview->setText(QString());
    m_preview->setPixmap(QPixmap::fromImage(
        previewImage(m_original, transformedSelection(), QSize(300, 190))));
}

} // namespace cc
