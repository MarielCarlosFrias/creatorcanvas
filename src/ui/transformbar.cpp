#include "transformbar.h"

#include "core/Document.h"
#include "localization/i18nservice.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>

#include <algorithm>

namespace cc {

AffineTransform numericResize(const AffineTransform& base, int naturalWidth,
                              int naturalHeight, double w, double h)
{
    AffineTransform t = base;
    if (naturalWidth <= 0 || naturalHeight <= 0)
        return t;
    const double newScaleX = std::max(1.0, w) / naturalWidth;
    const double newScaleY = std::max(1.0, h) / naturalHeight;
    t.scaleX = newScaleX * (base.scaleX < 0 ? -1.0 : 1.0);
    t.scaleY = newScaleY * (base.scaleY < 0 ? -1.0 : 1.0);
    return t;
}

QPair<double, double> lockAspect(double w, double h, int naturalWidth, int naturalHeight,
                                 bool fromWidth)
{
    if (naturalWidth <= 0 || naturalHeight <= 0)
        return {w, h};
    const double ratio = static_cast<double>(naturalWidth) / naturalHeight;
    if (fromWidth) {
        const double locked = std::max(1.0, w) / ratio;
        return {std::max(1.0, w), locked};
    }
    const double locked = std::max(1.0, h) * ratio;
    return {locked, std::max(1.0, h)};
}

TransformBar::TransformBar(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(6);

    auto makeLabel = [this](const QString& text) {
        auto* label = new QLabel(text, this);
        label->setStyleSheet(QStringLiteral("color: #9aa3b2; font-size: 11px;"));
        return label;
    };

    m_xSpin = new QDoubleSpinBox(this);
    m_ySpin = new QDoubleSpinBox(this);
    m_wSpin = new QDoubleSpinBox(this);
    m_hSpin = new QDoubleSpinBox(this);
    m_rotSpin = new QDoubleSpinBox(this);

    for (QDoubleSpinBox* s : {m_xSpin, m_ySpin, m_wSpin, m_hSpin, m_rotSpin}) {
        s->setDecimals(1);
        s->setRange(-100000.0, 100000.0);
        s->setKeyboardTracking(false);
        s->setMaximumWidth(84);
        connect(s, &QDoubleSpinBox::valueChanged, this, &TransformBar::onFieldEdited);
    }
    m_rotSpin->setRange(-360.0, 360.0);
    m_rotSpin->setSuffix(QStringLiteral("°"));

    m_aspectLock = new QCheckBox(this);
    m_aspectLock->setChecked(true);
    m_aspectLock->setToolTip(QStringLiteral("Proporção travada"));

    layout->addWidget(makeLabel(QStringLiteral("X")));
    layout->addWidget(m_xSpin);
    layout->addWidget(makeLabel(QStringLiteral("Y")));
    layout->addWidget(m_ySpin);
    layout->addWidget(makeLabel(QStringLiteral("L")));
    layout->addWidget(m_wSpin);
    layout->addWidget(makeLabel(QStringLiteral("A")));
    layout->addWidget(m_hSpin);
    layout->addWidget(m_aspectLock);
    layout->addWidget(makeLabel(QStringLiteral("∠")));
    layout->addWidget(m_rotSpin);
    layout->addStretch(1);

    retranslate();
}

void TransformBar::setI18n(I18nService* i18n)
{
    m_i18n = i18n;
    retranslate();
}

void TransformBar::retranslate()
{
    m_aspectLock->setToolTip(m_i18n ? m_i18n->t("editor", "transformBar.aspectLock")
                                    : QStringLiteral("Proporção travada"));
    m_aspectLock->setText(m_i18n ? m_i18n->t("editor", "transformBar.aspectShort")
                                 : QStringLiteral("🔗"));
}

void TransformBar::setLayer(const Document& doc, const LayerId& id)
{
    m_loading = true;

    const Layer* layer = id.isNull() ? nullptr : doc.findLayer(id);
    if (!layer) {
        m_id = LayerId();
        for (QDoubleSpinBox* s : {m_xSpin, m_ySpin, m_wSpin, m_hSpin, m_rotSpin})
            s->setEnabled(false);
        m_xSpin->setValue(0);
        m_ySpin->setValue(0);
        m_wSpin->setValue(0);
        m_hSpin->setValue(0);
        m_rotSpin->setValue(0);
        m_loading = false;
        return;
    }

    m_id = id;
    m_current = layer->transform;
    m_naturalWidth = 0;
    m_naturalHeight = 0;

    bool hasIntrinsicSize = false;
    if (layer->type() == LayerType::Image) {
        const auto* img = static_cast<const ImageLayer*>(layer);
        m_naturalWidth = img->naturalWidth;
        m_naturalHeight = img->naturalHeight;
        hasIntrinsicSize = m_naturalWidth > 0 && m_naturalHeight > 0;
    }

    const QPointF pos = layer->transform.position;
    m_xSpin->setValue(pos.x());
    m_ySpin->setValue(pos.y());
    m_rotSpin->setValue(layer->transform.rotationDeg);

    m_wSpin->setEnabled(hasIntrinsicSize);
    m_hSpin->setEnabled(hasIntrinsicSize);
    if (hasIntrinsicSize) {
        m_wSpin->setValue(std::max(1.0, m_naturalWidth * std::abs(layer->transform.scaleX)));
        m_hSpin->setValue(std::max(1.0, m_naturalHeight * std::abs(layer->transform.scaleY)));
    } else {
        m_wSpin->setValue(0);
        m_hSpin->setValue(0);
    }
    m_loading = false;
}

void TransformBar::onFieldEdited()
{
    if (m_loading || m_id.isNull())
        return;

    // Proporção: mexer num dos lados ajusta o outro.
    if (m_naturalWidth > 0 && m_naturalHeight > 0
        && (sender() == m_wSpin || sender() == m_hSpin) && m_aspectLock->isChecked()) {
        const bool fromWidth = sender() == m_wSpin;
        const auto [w, hh] = lockAspect(m_wSpin->value(), m_hSpin->value(),
                                        m_naturalWidth, m_naturalHeight, fromWidth);
        m_loading = true;
        m_wSpin->setValue(w);
        m_hSpin->setValue(hh);
        m_loading = false;
    }

    emitEdit();
}

void TransformBar::emitEdit()
{
    if (m_id.isNull())
        return;

    const AffineTransform oldTransform = m_current;
    AffineTransform t = m_current;
    t.position = QPointF(m_xSpin->value(), m_ySpin->value());
    t.rotationDeg = m_rotSpin->value();
    t = numericResize(m_current, m_naturalWidth, m_naturalHeight,
                      m_wSpin->value(), m_hSpin->value());
    m_current = t;
    emit transformEdited(m_id, oldTransform, t);
}

} // namespace cc
