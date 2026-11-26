#include "ui/imagesizedialog.h"

#include "core/Document.h"
#include "localization/i18nservice.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace cc {

ImageSizeDialog::ImageSizeDialog(const Document* document,
                                 I18nService* i18n, QWidget* parent)
    : QDialog(parent)
    , m_document(document)
    , m_i18n(i18n)
    , m_aspectRatio(document && document->height() > 0
                        ? double(document->width()) / document->height() : 1.0)
{
    buildUi();
    if (m_i18n) {
        connect(m_i18n, &I18nService::languageChanged,
                this, &ImageSizeDialog::retranslateUi);
    }
    retranslateUi();
}

ImageSizeDialog::ImageSizeDialog(const Document& document,
                                 I18nService* i18n, QWidget* parent)
    : ImageSizeDialog(&document, i18n, parent)
{
}

void ImageSizeDialog::buildUi()
{
    setModal(true);
    setMinimumWidth(360);

    auto* layout = new QVBoxLayout(this);
    m_currentSizeLabel = new QLabel(this);
    m_currentSizeLabel->setObjectName(QStringLiteral("currentSizeLabel"));
    layout->addWidget(m_currentSizeLabel);

    auto* form = new QFormLayout;
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    layout->addLayout(form);

    const int originalWidth = m_document ? m_document->width() : 1;
    const int originalHeight = m_document ? m_document->height() : 1;
    const int maximum = std::max({16384, originalWidth, originalHeight});

    m_width = new QSpinBox(this);
    m_width->setObjectName(QStringLiteral("widthSpinBox"));
    m_width->setRange(1, maximum);
    m_width->setValue(originalWidth);
    m_width->setSuffix(QStringLiteral(" px"));
    m_width->setAccelerated(true);

    m_height = new QSpinBox(this);
    m_height->setObjectName(QStringLiteral("heightSpinBox"));
    m_height->setRange(1, maximum);
    m_height->setValue(originalHeight);
    m_height->setSuffix(QStringLiteral(" px"));
    m_height->setAccelerated(true);

    m_widthLabel = new QLabel(this);
    m_heightLabel = new QLabel(this);
    form->addRow(m_widthLabel, m_width);
    form->addRow(m_heightLabel, m_height);

    m_constrainProportions = new QCheckBox(this);
    m_constrainProportions->setObjectName(
        QStringLiteral("constrainProportionsCheckBox"));
    m_constrainProportions->setChecked(true);
    form->addRow(QString(), m_constrainProportions);

    m_resample = new QComboBox(this);
    m_resample->setObjectName(QStringLiteral("resampleComboBox"));
    m_resampleLabel = new QLabel(this);
    form->addRow(m_resampleLabel, m_resample);

    m_scaleLabel = new QLabel(this);
    m_scaleLabel->setObjectName(QStringLiteral("scaleLabel"));
    layout->addWidget(m_scaleLabel);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(m_width, &QSpinBox::valueChanged,
            this, &ImageSizeDialog::widthChanged);
    connect(m_height, &QSpinBox::valueChanged,
            this, &ImageSizeDialog::heightChanged);
}

QString ImageSizeDialog::text(const QString& key, const QString& fallback) const
{
    return m_i18n ? m_i18n->t("common", key) : fallback;
}

void ImageSizeDialog::retranslateUi()
{
    setWindowTitle(text(QStringLiteral("imageSize.title"),
                        QStringLiteral("Image Size")));
    m_widthLabel->setText(text(QStringLiteral("imageSize.width"),
                               QStringLiteral("Width:")));
    m_heightLabel->setText(text(QStringLiteral("imageSize.height"),
                                QStringLiteral("Height:")));
    m_constrainProportions->setText(
        text(QStringLiteral("imageSize.constrainProportions"),
             QStringLiteral("Constrain proportions")));
    m_resampleLabel->setText(text(QStringLiteral("imageSize.resample"),
                                  QStringLiteral("Resample:")));

    const int method = m_resample->currentData().isValid()
        ? m_resample->currentData().toInt()
        : static_cast<int>(ResampleMethod::Bicubic);
    m_resample->blockSignals(true);
    m_resample->clear();
    m_resample->addItem(text(QStringLiteral("imageSize.resample.nearest"),
                             QStringLiteral("Nearest Neighbor (hard edges)")),
                        static_cast<int>(ResampleMethod::NearestNeighbor));
    m_resample->addItem(text(QStringLiteral("imageSize.resample.bilinear"),
                             QStringLiteral("Bilinear")),
                        static_cast<int>(ResampleMethod::Bilinear));
    m_resample->addItem(text(QStringLiteral("imageSize.resample.bicubic"),
                             QStringLiteral("Bicubic (smooth)")),
                        static_cast<int>(ResampleMethod::Bicubic));
    const int index = m_resample->findData(method);
    m_resample->setCurrentIndex(index >= 0 ? index : 2);
    m_resample->blockSignals(false);

    const int width = m_document ? m_document->width() : 1;
    const int height = m_document ? m_document->height() : 1;
    m_currentSizeLabel->setText(
        text(QStringLiteral("imageSize.currentSize"),
             QStringLiteral("Current size: %1 × %2 px"))
            .arg(width).arg(height));
    updateScaleLabel();
}

void ImageSizeDialog::widthChanged(int value)
{
    if (m_updatingDimensions)
        return;
    m_updatingDimensions = true;
    if (m_constrainProportions->isChecked() && m_aspectRatio > 0.0) {
        const int height = qBound(m_height->minimum(),
            static_cast<int>(std::lround(value / m_aspectRatio)),
            m_height->maximum());
        m_height->setValue(height);
    }
    m_updatingDimensions = false;
    updateScaleLabel();
}

void ImageSizeDialog::heightChanged(int value)
{
    if (m_updatingDimensions)
        return;
    m_updatingDimensions = true;
    if (m_constrainProportions->isChecked() && m_aspectRatio > 0.0) {
        const int width = qBound(m_width->minimum(),
            static_cast<int>(std::lround(value * m_aspectRatio)),
            m_width->maximum());
        m_width->setValue(width);
    }
    m_updatingDimensions = false;
    updateScaleLabel();
}

void ImageSizeDialog::updateScaleLabel()
{
    if (!m_scaleLabel || !m_width || !m_height)
        return;
    const double originalWidth = m_document ? m_document->width() : 1.0;
    const double originalHeight = m_document ? m_document->height() : 1.0;
    const double scaleX = originalWidth > 0.0
        ? 100.0 * m_width->value() / originalWidth : 100.0;
    const double scaleY = originalHeight > 0.0
        ? 100.0 * m_height->value() / originalHeight : 100.0;
    m_scaleLabel->setText(
        text(QStringLiteral("imageSize.scale"),
             QStringLiteral("Scale: %1% × %2%"))
            .arg(scaleX, 0, 'f', 1).arg(scaleY, 0, 'f', 1));
}

QSize ImageSizeDialog::imageSize() const
{
    return QSize(m_width->value(), m_height->value());
}

ResampleMethod ImageSizeDialog::resampleMethod() const
{
    return static_cast<ResampleMethod>(m_resample->currentData().toInt());
}

ImageResizeOptions ImageSizeDialog::options() const
{
    return ImageResizeOptions{imageSize(), resampleMethod()};
}

} // namespace cc
