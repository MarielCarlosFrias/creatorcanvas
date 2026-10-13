#include "exportdialog.h"

#include "core/Document.h"
#include "localization/i18nservice.h"
#include "rendering/CanvasRenderer.h"

#include <QBuffer>
#include <QButtonGroup>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QPainter>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace cc {

ExportDialog::ExportDialog(const Document* doc, I18nService* i18n,
                           const QUuid& selectedLayerId, QWidget* parent)
    : QDialog(parent)
    , m_doc(doc)
    , m_i18n(i18n)
    , m_selectedLayerId(selectedLayerId)
{
    buildUi();
    if (m_i18n) {
        connect(m_i18n, &I18nService::languageChanged,
                this, &ExportDialog::retranslateUi);
    }
    retranslateUi();
    updateQualityEnabled();
    updatePreview();
}

void ExportDialog::buildUi()
{
    setMinimumSize(520, 360);
    setMaximumSize(1200, 900);
    resize(700, 440);
    setSizeGripEnabled(true);
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setSizeConstraint(QLayout::SetMinimumSize);
    mainLayout->setSpacing(16);

    // Left Column: Configuration Controls
    auto* leftWidget = new QWidget(this);
    auto* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    // Scope selection (Entire Canvas vs Selected Layer)
    auto* scopeBox = new QGroupBox(this);
    auto* scopeLayout = new QHBoxLayout(scopeBox);
    m_scopeEntire = new QRadioButton(this);
    m_scopeEntire->setChecked(true);
    m_scopeLayer = new QRadioButton(this);
    m_scopeGroup = new QButtonGroup(this);
    m_scopeGroup->addButton(m_scopeEntire, 0);
    m_scopeGroup->addButton(m_scopeLayer, 1);
    scopeLayout->addWidget(m_scopeEntire);
    scopeLayout->addWidget(m_scopeLayer);

    const bool hasSelectedLayer = m_doc && !m_selectedLayerId.isNull() &&
                                  m_doc->findLayer(m_selectedLayerId) != nullptr;
    m_scopeLayer->setEnabled(hasSelectedLayer);

    leftLayout->addWidget(scopeBox);

    auto* form = new QFormLayout;
    form->setSpacing(10);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);

    // Presets dropdown
    m_presetCombo = new QComboBox(this);
    m_presetCombo->addItem(QStringLiteral("Original"), QStringLiteral("original"));
    m_presetCombo->addItem(QStringLiteral("YouTube Thumbnail (1280x720)"), QStringLiteral("yt_thumb"));
    m_presetCombo->addItem(QStringLiteral("Instagram Post (1080x1080)"), QStringLiteral("ig_post"));
    m_presetCombo->addItem(QStringLiteral("Story / TikTok (1080x1920)"), QStringLiteral("story"));
    m_presetCombo->addItem(QStringLiteral("Twitter/X Banner (1500x500)"), QStringLiteral("x_banner"));

    m_presetLabel = new QLabel(this);
    form->addRow(m_presetLabel, m_presetCombo);

    // Format
    m_format = new QComboBox(this);
    m_format->addItem(QStringLiteral("PNG"), QStringLiteral("png"));
    m_format->addItem(QStringLiteral("JPEG"), QStringLiteral("jpeg"));
    m_format->addItem(QStringLiteral("WebP"), QStringLiteral("webp"));

    m_formatLabel = new QLabel(this);
    form->addRow(m_formatLabel, m_format);

    // Quality (for JPEG / WebP)
    m_quality = new QSpinBox(this);
    m_quality->setRange(1, 100);
    m_quality->setValue(92);

    m_qualityLabel = new QLabel(this);
    form->addRow(m_qualityLabel, m_quality);

    // Scale
    m_scale = new QComboBox(this);
    m_scale->addItem(QStringLiteral("50%"), 0.5);
    m_scale->addItem(QStringLiteral("100%"), 1.0);
    m_scale->addItem(QStringLiteral("150%"), 1.5);
    m_scale->addItem(QStringLiteral("200%"), 2.0);
    m_scale->setCurrentIndex(1); // 100%

    m_scaleLabel = new QLabel(this);
    form->addRow(m_scaleLabel, m_scale);

    leftLayout->addLayout(form);
    leftLayout->addStretch();

    // Dialog Button Box
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    leftLayout->addWidget(buttons);

    // Right Column: Live Preview & File Size Estimation (inspired by PhotoCraft)
    auto* rightWidget = new QWidget(this);
    auto* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    auto* previewFrame = new QFrame(this);
    previewFrame->setFrameShape(QFrame::StyledPanel);
    previewFrame->setStyleSheet(QStringLiteral("background-color: #202124; border-radius: 6px;"));
    auto* frameLayout = new QVBoxLayout(previewFrame);
    frameLayout->setAlignment(Qt::AlignCenter);

    m_previewImageLabel = new QLabel(previewFrame);
    m_previewImageLabel->setMinimumSize(160, 120);
    m_previewImageLabel->setSizePolicy(QSizePolicy::Expanding,
                                       QSizePolicy::Expanding);
    m_previewImageLabel->setAlignment(Qt::AlignCenter);
    frameLayout->addWidget(m_previewImageLabel);

    rightLayout->addWidget(previewFrame);

    auto* infoLayout = new QHBoxLayout;
    m_dimensionLabel = new QLabel(this);
    m_dimensionLabel->setStyleSheet(QStringLiteral("font-weight: bold; color: #e8eaed;"));

    m_fileSizeLabel = new QLabel(this);
    m_fileSizeLabel->setStyleSheet(QStringLiteral("color: #8ab4f8; font-weight: bold;"));

    infoLayout->addWidget(m_dimensionLabel);
    infoLayout->addStretch();
    infoLayout->addWidget(m_fileSizeLabel);

    rightLayout->addLayout(infoLayout);

    mainLayout->addWidget(leftWidget, 1);
    mainLayout->addWidget(rightWidget, 1);

    // Signal connections
    connect(m_presetCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &ExportDialog::updatePreset);
    connect(m_format, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this]() {
                updateQualityEnabled();
                estimateFileSize();
            });
    connect(m_quality, qOverload<int>(&QSpinBox::valueChanged),
            this, &ExportDialog::estimateFileSize);
    connect(m_scale, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this]() {
                updatePreview();
            });
    connect(m_scopeGroup, &QButtonGroup::idClicked,
            this, [this]() {
                updatePreview();
            });
}

void ExportDialog::updateQualityEnabled()
{
    const QString fmt = m_format->currentData().toString();
    m_quality->setEnabled(fmt != QLatin1String("png"));
}

void ExportDialog::updatePreset()
{
    const QString preset = m_presetCombo->currentData().toString();
    if (!m_doc || preset == QLatin1String("original"))
        return;

    const double docW = m_doc->width();
    const double docH = m_doc->height();
    if (docW <= 0 || docH <= 0)
        return;

    double targetW = docW;
    if (preset == QLatin1String("yt_thumb")) {
        targetW = 1280.0;
        m_format->setCurrentIndex(1); // JPEG
        m_quality->setValue(92);
    } else if (preset == QLatin1String("ig_post")) {
        targetW = 1080.0;
        m_format->setCurrentIndex(1); // JPEG
        m_quality->setValue(90);
    } else if (preset == QLatin1String("story")) {
        targetW = 1080.0;
        m_format->setCurrentIndex(0); // PNG
    } else if (preset == QLatin1String("x_banner")) {
        targetW = 1500.0;
        m_format->setCurrentIndex(0); // PNG
    }

    const double computedScale = targetW / docW;
    // Check if scale already exists in combo or add it
    int foundIndex = -1;
    for (int i = 0; i < m_scale->count(); ++i) {
        if (qAbs(m_scale->itemData(i).toDouble() - computedScale) < 0.05) {
            foundIndex = i;
            break;
        }
    }
    if (foundIndex != -1) {
        m_scale->setCurrentIndex(foundIndex);
    } else {
        const QString label = QString::number(qRound(computedScale * 100)) + QStringLiteral("%");
        m_scale->addItem(label, computedScale);
        m_scale->setCurrentIndex(m_scale->count() - 1);
    }
}

void ExportDialog::updatePreview()
{
    if (!m_doc)
        return;

    const bool layerOnly = m_scopeLayer->isChecked() && !m_selectedLayerId.isNull();
    const double scale = m_scale->currentData().toDouble();

    const int outWidth = qMax(1, static_cast<int>(m_doc->width() * scale));
    const int outHeight = qMax(1, static_cast<int>(m_doc->height() * scale));
    m_dimensionLabel->setText(QStringLiteral("%1 x %2 px").arg(outWidth).arg(outHeight));

    // Render a 300x240 preview with checkerboard transparency
    const int maxThumbSide = 280;
    const double previewScale = qMin(static_cast<double>(maxThumbSide) / m_doc->width(),
                                      static_cast<double>(maxThumbSide) / m_doc->height());

    const int thumbW = qMax(1, static_cast<int>(m_doc->width() * previewScale));
    const int thumbH = qMax(1, static_cast<int>(m_doc->height() * previewScale));

    QImage thumbImage(thumbW, thumbH, QImage::Format_ARGB32);
    thumbImage.fill(Qt::transparent);

    {
        QPainter p(&thumbImage);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        QTransform t;
        t.scale(previewScale, previewScale);

        RenderOptions opts;
        opts.drawCheckerboard = true;
        opts.checkerSize = 8;
        opts.canvasBorder = Qt::transparent;

        if (layerOnly) {
            renderSiblingLayers(*m_doc, {m_selectedLayerId}, &p, opts);
        } else {
            renderDocument(*m_doc, &p, t, opts);
        }
    }

    m_cachedBaseThumbnail = thumbImage;
    m_previewImageLabel->setPixmap(QPixmap::fromImage(thumbImage));

    estimateFileSize();
}

void ExportDialog::estimateFileSize()
{
    if (!m_doc || m_cachedBaseThumbnail.isNull()) {
        m_fileSizeLabel->setText(QStringLiteral("~ -- KB"));
        return;
    }

    const QString fmt = m_format->currentData().toString();
    const int quality = m_quality->value();
    const double scale = m_scale->currentData().toDouble();

    // PhotoCraft algorithm: Encode the small proxy thumbnail in memory,
    // then scale the byte size by pixel ratio (full_pixels / proxy_pixels).
    QByteArray buffer;
    QBuffer qb(&buffer);
    qb.open(QIODevice::WriteOnly);

    QImage proxy = m_cachedBaseThumbnail;
    if (fmt == QLatin1String("jpeg")) {
        QImage flat(proxy.size(), QImage::Format_RGB32);
        flat.fill(Qt::white);
        QPainter p(&flat);
        p.drawImage(0, 0, proxy);
        p.end();
        flat.save(&qb, "JPEG", quality);
    } else if (fmt == QLatin1String("webp")) {
        proxy.save(&qb, "WEBP", quality);
    } else {
        proxy.save(&qb, "PNG");
    }

    const qint64 proxyBytes = buffer.size();
    const double fullPixels = (m_doc->width() * scale) * (m_doc->height() * scale);
    const double proxyPixels = qMax(1, proxy.width() * proxy.height());
    const double estimatedBytes = static_cast<double>(proxyBytes) * (fullPixels / proxyPixels);

    if (estimatedBytes < 1024 * 1024) {
        m_fileSizeLabel->setText(QStringLiteral("Est: ~%1 KB").arg(qRound(estimatedBytes / 1024.0)));
    } else {
        m_fileSizeLabel->setText(QStringLiteral("Est: ~%1 MB").arg(estimatedBytes / (1024.0 * 1024.0), 0, 'f', 1));
    }
}

ExportSettings ExportDialog::settings() const
{
    ExportSettings s;
    s.format = m_format->currentData().toString();
    s.quality = m_quality->value();
    s.scale = m_scale->currentData().toDouble();
    s.exportSelectedLayerOnly = m_scopeLayer->isChecked();
    s.selectedLayerId = m_selectedLayerId;
    return s;
}

void ExportDialog::retranslateUi()
{
    if (!m_i18n)
        return;
    setWindowTitle(m_i18n->t("editor", "export.dialog.title"));
    m_formatLabel->setText(m_i18n->t("editor", "export.format"));
    m_qualityLabel->setText(m_i18n->t("editor", "export.quality"));
    m_scaleLabel->setText(m_i18n->t("editor", "export.scale"));
    m_presetLabel->setText(m_i18n->t("editor", "export.preset"));
    m_scopeEntire->setText(m_i18n->t("editor", "export.scopeEntire"));
    m_scopeLayer->setText(m_i18n->t("editor", "export.scopeLayer"));
}

} // namespace cc
