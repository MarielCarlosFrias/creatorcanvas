#include "exportdialog.h"

#include "core/Document.h"
#include "localization/i18nservice.h"
#include "rendering/CanvasRenderer.h"
#include "themeicons.h"

#include <QBuffer>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

namespace cc {

namespace {

constexpr int kPreviewThumbSide = 320;
constexpr int kDebounceMs = 150;

QLabel* makeCaption(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setStyleSheet(QStringLiteral(
        "color: #9aa0a6; font-size: 10px; font-weight: bold;"));
    return label;
}

} // namespace

ExportDialog::ExportDialog(const Document* doc, I18nService* i18n,
                           const QUuid& selectedLayerId, QWidget* parent)
    : QDialog(parent)
    , m_doc(doc)
    , m_i18n(i18n)
    , m_selectedLayerId(selectedLayerId)
{
    setMinimumSize(660, 460);
    resize(840, 520);
    buildUi();
    if (m_i18n) {
        connect(m_i18n, &I18nService::languageChanged,
                this, &ExportDialog::retranslateUi);
    }

    // Debounce timer: heavy preview render runs only after the user stops
    // changing settings for ~150 ms, avoiding continuous expensive renders.
    m_previewDebounceTimer = new QTimer(this);
    m_previewDebounceTimer->setSingleShot(true);
    m_previewDebounceTimer->setInterval(kDebounceMs);
    connect(m_previewDebounceTimer, &QTimer::timeout, this,
            &ExportDialog::updatePreview);

    retranslateUi();
    updateControlsEnabled();
}

void ExportDialog::buildUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 14);
    mainLayout->setSpacing(14);

    auto* contentLayout = new QHBoxLayout;
    contentLayout->setSpacing(20);
    mainLayout->addLayout(contentLayout, 1);

    // ------------------------------------------------------------------
    // Left column: scope, quick presets and output options
    // ------------------------------------------------------------------
    auto* leftWidget = new QWidget(this);
    auto* leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(8);

    m_scopeCaption = makeCaption(QString(), leftWidget);
    leftLayout->addWidget(m_scopeCaption);

    // Scope selection: segmented control (Entire Canvas vs Selected Layer)
    auto* scopeRow = new QHBoxLayout;
    scopeRow->setSpacing(0);
    m_scopeEntire = new QRadioButton(leftWidget);
    m_scopeEntire->setChecked(true);
    m_scopeEntire->setCursor(Qt::PointingHandCursor);
    m_scopeLayer = new QRadioButton(leftWidget);
    m_scopeLayer->setCursor(Qt::PointingHandCursor);
    const QString segmentedStyle = QStringLiteral(
        "QRadioButton {"
        "  background-color: #2b2d30;"
        "  color: #e8eaed;"
        "  border: 1px solid #3c4043;"
        "  border-radius: 6px;"
        "  padding: 7px 14px;"
        "  font-size: 12px;"
        "}"
        "QRadioButton:hover { background-color: #35373b; border-color: #5f6368; }"
        "QRadioButton:checked {"
        "  background-color: #1a73e8;"
        "  border-color: #1a73e8;"
        "  color: #ffffff;"
        "  font-weight: bold;"
        "}"
        "QRadioButton:disabled { color: #6b7075; border-color: #33363a; }"
        "QRadioButton::indicator { width: 0px; height: 0px; }");
    m_scopeEntire->setStyleSheet(segmentedStyle);
    m_scopeLayer->setStyleSheet(segmentedStyle);
    m_scopeGroup = new QButtonGroup(this);
    m_scopeGroup->addButton(m_scopeEntire, 0);
    m_scopeGroup->addButton(m_scopeLayer, 1);
    scopeRow->addWidget(m_scopeEntire);
    scopeRow->addWidget(m_scopeLayer);
    scopeRow->addStretch();

    const bool hasSelectedLayer = m_doc && !m_selectedLayerId.isNull() &&
                                  m_doc->findLayer(m_selectedLayerId) != nullptr;
    m_scopeLayer->setEnabled(hasSelectedLayer);
    if (hasSelectedLayer && m_doc) {
        if (const auto* layer = m_doc->findLayer(m_selectedLayerId)) {
            if (!layer->name.trimmed().isEmpty())
                m_scopeLayer->setToolTip(layer->name.trimmed());
        }
    }

    leftLayout->addLayout(scopeRow);
    leftLayout->addSpacing(10);

    m_presetsCaption = makeCaption(QString(), leftWidget);
    leftLayout->addWidget(m_presetsCaption);

    // Quick presets chips bar (Original, Instagram, Story, YouTube, X)
    auto* chipsContainer = new QWidget(leftWidget);
    auto* chipsLayout = new QHBoxLayout(chipsContainer);
    chipsLayout->setContentsMargins(0, 0, 0, 0);
    chipsLayout->setSpacing(8);

    m_presetChipsGroup = new QButtonGroup(this);
    m_presetChipsGroup->setExclusive(false);

    auto makePresetChip = [this, chipsLayout, chipsContainer](const QString& iconKey,
                                                              const QString& presetId) {
        auto* chip = new QPushButton(chipsContainer);
        chip->setCheckable(true);
        chip->setCursor(Qt::PointingHandCursor);
        chip->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  background-color: #2b2d30;"
            "  color: #e8eaed;"
            "  border: 1px solid #3c4043;"
            "  border-radius: 14px;"
            "  padding: 6px 12px;"
            "  font-size: 11px;"
            "}"
            "QPushButton:hover {"
            "  background-color: #35373b;"
            "  border-color: #5f6368;"
            "}"
            "QPushButton:checked {"
            "  background-color: #1a73e8;"
            "  color: #ffffff;"
            "  border-color: #1a73e8;"
            "  font-weight: bold;"
            "}"));
        m_presetChipsGroup->addButton(chip);
        chipsLayout->addWidget(chip);
        m_presetChips.insert(presetId, chip);
        connect(chip, &QPushButton::clicked, this, [this, presetId] {
            applyPresetById(presetId);
        });
        (void)iconKey;
        return chip;
    };

    makePresetChip(QString(), QStringLiteral("original"));
    makePresetChip(QStringLiteral("instagram"), QStringLiteral("ig_post"));
    makePresetChip(QStringLiteral("story"), QStringLiteral("story"));
    makePresetChip(QStringLiteral("youtube"), QStringLiteral("yt_thumb"));
    makePresetChip(QStringLiteral("banner"), QStringLiteral("x_banner"));
    chipsLayout->addStretch();

    leftLayout->addWidget(chipsContainer);
    leftLayout->addSpacing(6);

    m_outputCaption = makeCaption(QString(), leftWidget);
    leftLayout->addWidget(m_outputCaption);

    // Output options (format, quality, scale, transparency)
    auto* outputCard = new QFrame(leftWidget);
    outputCard->setStyleSheet(QStringLiteral(
        "QFrame#outputCard {"
        "  background-color: #202124;"
        "  border: 1px solid #3c4043;"
        "  border-radius: 8px;"
        "}"));
    outputCard->setObjectName(QStringLiteral("outputCard"));
    auto* outputLayout = new QVBoxLayout(outputCard);
    outputLayout->setContentsMargins(16, 14, 16, 14);
    outputLayout->setSpacing(12);

    const QString fieldLabelStyle = QStringLiteral("color: #e8eaed; font-size: 12px;");
    const QString comboStyle = QStringLiteral(
        "QComboBox {"
        "  background-color: #2b2d30;"
        "  color: #e8eaed;"
        "  border: 1px solid #3c4043;"
        "  border-radius: 6px;"
        "  padding: 5px 10px;"
        "  min-width: 140px;"
        "  font-size: 12px;"
        "}"
        "QComboBox:hover { border-color: #5f6368; }"
        "QComboBox::drop-down { border: none; width: 18px; }"
        "QComboBox QAbstractItemView {"
        "  background-color: #2b2d30;"
        "  color: #e8eaed;"
        "  border: 1px solid #3c4043;"
        "  selection-background-color: #1a73e8;"
        "}");
    const QString spinStyle = QStringLiteral(
        "QSpinBox {"
        "  background-color: #2b2d30;"
        "  color: #e8eaed;"
        "  border: 1px solid #3c4043;"
        "  border-radius: 6px;"
        "  padding: 5px 8px;"
        "  min-width: 70px;"
        "  font-size: 12px;"
        "}"
        "QSpinBox:hover { border-color: #5f6368; }"
        "QSpinBox:disabled { color: #6b7075; }");

    auto addFieldRow = [&](QLabel*& labelOut, QWidget* control) {
        auto* row = new QHBoxLayout;
        labelOut = new QLabel(outputCard);
        labelOut->setStyleSheet(fieldLabelStyle);
        row->addWidget(labelOut);
        row->addStretch();
        row->addWidget(control);
        outputLayout->addLayout(row);
        return row;
    };

    m_format = new QComboBox(outputCard);
    m_format->setStyleSheet(comboStyle);
    m_format->addItem(QStringLiteral("PNG"), QStringLiteral("png"));
    m_format->addItem(QStringLiteral("JPEG"), QStringLiteral("jpeg"));
    m_format->addItem(QStringLiteral("WebP"), QStringLiteral("webp"));
    // Ambos podem conter vectores e imagens; não prometer PDF/SVG 100%
    // vectoriais quando efeitos/composição ou bitmaps exigem raster.
    m_format->addItem(QStringLiteral("PDF (vetorial/raster)"), QStringLiteral("pdf"));
    m_format->addItem(QStringLiteral("SVG (vetorial + raster)"), QStringLiteral("svg"));
    addFieldRow(m_formatLabel, m_format);

    m_quality = new QSpinBox(outputCard);
    m_quality->setRange(1, 100);
    m_quality->setValue(92);
    m_quality->setStyleSheet(spinStyle);
    addFieldRow(m_qualityLabel, m_quality);

    m_scale = new QComboBox(outputCard);
    m_scale->setStyleSheet(comboStyle);
    m_scale->addItem(QStringLiteral("50%"), 0.5);
    m_scale->addItem(QStringLiteral("100%"), 1.0);
    m_scale->addItem(QStringLiteral("150%"), 1.5);
    m_scale->addItem(QStringLiteral("200%"), 2.0);
    m_scale->setCurrentIndex(m_scale->findData(1.0));
    addFieldRow(m_scaleLabel, m_scale);

    // Transparent background: only meaningful when the format keeps alpha.
    m_transparent = new QCheckBox(outputCard);
    m_transparent->setChecked(true);
    m_transparent->setCursor(Qt::PointingHandCursor);
    m_transparent->setStyleSheet(QStringLiteral(
        "QCheckBox { color: #e8eaed; font-size: 12px; spacing: 8px; }"
        "QCheckBox::indicator {"
        "  width: 16px; height: 16px;"
        "  border: 1px solid #5f6368;"
        "  border-radius: 4px;"
        "  background-color: #2b2d30;"
        "}"
        "QCheckBox::indicator:checked {"
        "  background-color: #1a73e8;"
        "  border-color: #1a73e8;"
        "  image: none;"
        "}"
        "QCheckBox:disabled { color: #6b7075; }"));
    auto* transparentRow = new QHBoxLayout;
    m_transparentLabel = new QLabel(outputCard);
    m_transparentLabel->setStyleSheet(fieldLabelStyle);
    m_transparentLabel->setBuddy(m_transparent);
    transparentRow->addWidget(m_transparentLabel);
    transparentRow->addStretch();
    transparentRow->addWidget(m_transparent);
    outputLayout->addLayout(transparentRow);

    leftLayout->addWidget(outputCard);
    leftLayout->addStretch();

    // ------------------------------------------------------------------
    // Right column: live preview and estimated file size
    // ------------------------------------------------------------------
    auto* rightWidget = new QWidget(this);
    auto* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(8);

    m_previewCaption = makeCaption(QString(), rightWidget);
    rightLayout->addWidget(m_previewCaption);

    auto* previewFrame = new QFrame(rightWidget);
    previewFrame->setObjectName(QStringLiteral("previewFrame"));
    previewFrame->setStyleSheet(QStringLiteral(
        "QFrame#previewFrame {"
        "  background-color: #17181a;"
        "  border: 1px solid #3c4043;"
        "  border-radius: 8px;"
        "}"));
    previewFrame->setFrameShape(QFrame::StyledPanel);
    auto* frameLayout = new QVBoxLayout(previewFrame);
    frameLayout->setContentsMargins(12, 12, 12, 12);
    frameLayout->setAlignment(Qt::AlignCenter);

    m_previewImageLabel = new QLabel(previewFrame);
    m_previewImageLabel->setMinimumSize(180, 140);
    m_previewImageLabel->setSizePolicy(QSizePolicy::Expanding,
                                       QSizePolicy::Expanding);
    m_previewImageLabel->setAlignment(Qt::AlignCenter);
    frameLayout->addWidget(m_previewImageLabel);

    rightLayout->addWidget(previewFrame, 1);

    auto* infoLayout = new QHBoxLayout;
    infoLayout->setSpacing(8);
    m_dimensionLabel = new QLabel(rightWidget);
    m_fileSizeLabel = new QLabel(rightWidget);
    m_dimensionLabel->setStyleSheet(QStringLiteral(
        "background-color: #2b2d30; border: 1px solid #3c4043; border-radius: 10px;"
        " color: #e8eaed; font-size: 11px; font-weight: bold; padding: 4px 10px;"));
    m_fileSizeLabel->setStyleSheet(QStringLiteral(
        "background-color: #2b2d30; border: 1px solid #3c4043; border-radius: 10px;"
        " color: #8ab4f8; font-size: 11px; font-weight: bold; padding: 4px 10px;"));
    infoLayout->addWidget(m_dimensionLabel);
    infoLayout->addStretch();
    infoLayout->addWidget(m_fileSizeLabel);
    rightLayout->addLayout(infoLayout);

    contentLayout->addWidget(leftWidget, 3);
    contentLayout->addWidget(rightWidget, 2);

    // ------------------------------------------------------------------
    // Bottom dialog buttons
    // ------------------------------------------------------------------
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Cancel | QDialogButtonBox::Ok, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);

    // ------------------------------------------------------------------
    // Signal connections
    // ------------------------------------------------------------------
    connect(m_format, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this]() {
                updateControlsEnabled();
                if (!m_applyingPreset) {
                    m_currentPreset = QStringLiteral("custom");
                    updatePresetChips();
                }
                estimateFileSize();
            });
    connect(m_quality, qOverload<int>(&QSpinBox::valueChanged),
            this, [this]() {
                if (!m_applyingPreset) {
                    m_currentPreset = QStringLiteral("custom");
                    updatePresetChips();
                }
                estimateFileSize();
            });
    connect(m_scale, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this]() {
                if (!m_applyingPreset) {
                    m_currentPreset = QStringLiteral("custom");
                    updatePresetChips();
                }
                m_previewDebounceTimer->start();
            });
    connect(m_transparent, &QCheckBox::toggled, this, [this](bool checked) {
        // Programmatic (un)checks while the format lacks alpha must not
        // overwrite the remembered choice: those happen while disabled.
        if (m_transparent->isEnabled())
            m_transparentWanted = checked;
        m_previewDebounceTimer->start();
    });
    connect(m_scopeGroup, &QButtonGroup::idClicked,
            this, [this]() {
                m_previewDebounceTimer->start();
            });
}

void ExportDialog::updateControlsEnabled()
{
    if (!m_format || !m_quality || !m_transparent)
        return;

    const QString fmt = m_format->currentData().toString();
    m_quality->setEnabled(fmt != QLatin1String("png") && fmt != QLatin1String("pdf")
                          && fmt != QLatin1String("svg"));
    // PDF/SVG exportam o documento inteiro (vectores e/ou raster): o escopo
    // "só camada" não se aplica.
    const bool wholeDocOnly = (fmt == QLatin1String("pdf") || fmt == QLatin1String("svg"));
    if (m_scopeLayer) {
        const bool hasSelectedLayer = m_doc && !m_selectedLayerId.isNull()
                                      && m_doc->findLayer(m_selectedLayerId) != nullptr;
        m_scopeLayer->setEnabled(!wholeDocOnly && hasSelectedLayer);
        if (wholeDocOnly && m_scopeLayer->isChecked() && m_scopeEntire)
            m_scopeEntire->setChecked(true);
    }

    // Só PNG/WebP expõem a opção de flatten do backend. JPEG é sempre opaco;
    // PDF/SVG preservam transparência, mas não têm um argumento para a desligar.
    // O checkbox reflecte cada comportamento sem oferecer um controlo no-op.
    const bool hasTransparencyToggle = fmt == QLatin1String("png")
                                       || fmt == QLatin1String("webp");
    const bool inherentlyTransparent = fmt == QLatin1String("pdf")
                                       || fmt == QLatin1String("svg");
    m_transparent->setEnabled(hasTransparencyToggle);
    const bool checked = hasTransparencyToggle ? m_transparentWanted
                                               : inherentlyTransparent;
    if (m_transparent->isChecked() != checked)
        m_transparent->setChecked(checked);
    if (m_transparentLabel)
        m_transparentLabel->setEnabled(hasTransparencyToggle);

    if (inherentlyTransparent) {
        m_transparent->setToolTip(tr_(QStringLiteral("export.transparentVectorHint"),
                                      QStringLiteral("PDF and SVG preserve source transparency.")));
    } else if (fmt == QLatin1String("jpeg")) {
        m_transparent->setToolTip(tr_(QStringLiteral("export.transparentJpegHint"),
                                      QStringLiteral("JPEG is always flattened onto white.")));
    } else {
        m_transparent->setToolTip(QString());
    }
}

void ExportDialog::applyPresetById(const QString& presetId)
{
    applyPreset(presetId);
}

void ExportDialog::applyPreset(const QString& presetId)
{
    m_applyingPreset = true;
    m_currentPreset = presetId;
    updatePresetChips();

    if (presetId == QLatin1String("original")) {
        for (int i = 0; i < m_scale->count(); ++i) {
            if (qAbs(m_scale->itemData(i).toDouble() - 1.0) < 0.05) {
                m_scale->setCurrentIndex(i);
                break;
            }
        }
        m_applyingPreset = false;
        updateControlsEnabled();
        m_previewDebounceTimer->start();
        return;
    }

    if (!m_doc) {
        m_applyingPreset = false;
        return;
    }

    const double docW = m_doc->width();
    const double docH = m_doc->height();
    if (docW <= 0 || docH <= 0) {
        m_applyingPreset = false;
        return;
    }

    double targetW = docW;
    if (presetId == QLatin1String("yt_thumb")) {
        targetW = 1280.0;
        m_format->setCurrentIndex(m_format->findData(QStringLiteral("jpeg")));
        m_quality->setValue(92);
    } else if (presetId == QLatin1String("ig_post")) {
        targetW = 1080.0;
        m_format->setCurrentIndex(m_format->findData(QStringLiteral("jpeg")));
        m_quality->setValue(90);
    } else if (presetId == QLatin1String("story")) {
        targetW = 1080.0;
        m_format->setCurrentIndex(m_format->findData(QStringLiteral("png")));
    } else if (presetId == QLatin1String("x_banner")) {
        targetW = 1500.0;
        m_format->setCurrentIndex(m_format->findData(QStringLiteral("png")));
    }

    const double computedScale = targetW / docW;
    const QString label = QString::number(qRound(computedScale * 100)) +
                          QStringLiteral("%");

    // Reuse a single dynamic slot so repeated preset switching does not
    // accumulate duplicated scale entries.
    if (m_dynamicScaleIndex < 0) {
        m_dynamicScaleIndex = m_scale->count();
        m_scale->addItem(label, computedScale);
    } else {
        m_scale->setItemText(m_dynamicScaleIndex, label);
        m_scale->setItemData(m_dynamicScaleIndex, computedScale);
    }
    m_scale->setCurrentIndex(m_dynamicScaleIndex);

    m_applyingPreset = false;
    updateControlsEnabled();
    m_previewDebounceTimer->start();
}

void ExportDialog::updatePresetChips()
{
    for (auto* chip : m_presetChips)
        chip->setChecked(false);
    if (auto* chip = m_presetChips.value(m_currentPreset, nullptr))
        chip->setChecked(true);
}

void ExportDialog::updatePreview()
{
    if (!m_doc)
        return;

    const bool layerOnly = m_scopeLayer->isChecked() && !m_selectedLayerId.isNull();
    const double scale = m_scale->currentData().toDouble();

    const int outWidth = qMax(1, static_cast<int>(m_doc->width() * scale));
    const int outHeight = qMax(1, static_cast<int>(m_doc->height() * scale));
    if (m_i18n) {
        m_dimensionLabel->setText(
            m_i18n->t("editor", QStringLiteral("export.dimension"))
                .arg(outWidth).arg(outHeight));
    } else {
        m_dimensionLabel->setText(
            QStringLiteral("%1 x %2 px").arg(outWidth).arg(outHeight));
    }

    // Render once WITHOUT checkerboard: this is the same alpha content used by
    // the raster backends and therefore also by the size estimator.
    const double previewScale = qMin(static_cast<double>(kPreviewThumbSide) / m_doc->width(),
                                     static_cast<double>(kPreviewThumbSide) / m_doc->height());

    const int thumbW = qMax(1, static_cast<int>(m_doc->width() * previewScale));
    const int thumbH = qMax(1, static_cast<int>(m_doc->height() * previewScale));

    QImage thumbImage(thumbW, thumbH, QImage::Format_ARGB32_Premultiplied);
    thumbImage.fill(Qt::transparent);

    {
        QPainter p(&thumbImage);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::TextAntialiasing, true);
        QTransform t;
        t.scale(previewScale, previewScale);

        RenderOptions opts;
        opts.drawCheckerboard = false;
        opts.canvasBorder = Qt::transparent;

        if (layerOnly) {
            // renderSiblingLayers uses the painter's current transform (unlike
            // renderDocument, which receives it as an argument).
            p.setTransform(t);
            renderSiblingLayers(*m_doc, {m_selectedLayerId}, &p, opts);
        } else {
            renderDocument(*m_doc, &p, t, opts);
        }
    }

    m_cachedBaseThumbnail = thumbImage;

    // Checkerboard/white belongs only to the display surface. Mixing it into
    // the cached source made transparent PNG/WebP estimates encode a fake
    // opaque checker pattern that the exporters never write.
    QImage displayImage(thumbImage.size(), QImage::Format_RGB32);
    displayImage.fill(Qt::white);
    {
        QPainter display(&displayImage);
        if (m_transparent->isChecked()) {
            constexpr int checkerSize = 8;
            for (int y = 0; y < displayImage.height(); y += checkerSize) {
                for (int x = 0; x < displayImage.width(); x += checkerSize) {
                    const bool dark = ((x / checkerSize) + (y / checkerSize)) % 2 != 0;
                    display.fillRect(QRect(x, y, checkerSize, checkerSize),
                                     dark ? QColor(0x99, 0x99, 0x99)
                                          : QColor(0xcc, 0xcc, 0xcc));
                }
            }
        }
        display.drawImage(0, 0, thumbImage);
    }
    m_previewImageLabel->setPixmap(QPixmap::fromImage(displayImage));

    estimateFileSize();
}

QString ExportDialog::tr_(const QString& key, const QString& fallback) const
{
    if (!m_i18n)
        return fallback;
    const QString t = m_i18n->t("editor", key);
    // A total miss returns the dotted key; fall back to English then.
    return (t == QStringLiteral("editor.") + key) ? fallback : t;
}

QString ExportDialog::presetTooltipKey(const QString& presetId)
{
    if (presetId == QLatin1String("ig_post"))
        return QStringLiteral("export.preset.igPost");
    if (presetId == QLatin1String("story"))
        return QStringLiteral("export.preset.story");
    if (presetId == QLatin1String("yt_thumb"))
        return QStringLiteral("export.preset.ytThumb");
    if (presetId == QLatin1String("x_banner"))
        return QStringLiteral("export.preset.xBanner");
    return QStringLiteral("export.preset.original");
}

void ExportDialog::estimateFileSize()
{
    if (!m_doc || m_cachedBaseThumbnail.isNull()) {
        m_fileSizeLabel->setText(
            tr_(QStringLiteral("export.fileSizeUnknown"), QStringLiteral("~ -- KB")));
        return;
    }

    const QString fmt = m_format->currentData().toString();
    if (fmt == QLatin1String("pdf") || fmt == QLatin1String("svg")) {
        // Um PNG de proxy multiplicado pelo número de píxeis não estima um
        // content stream PDF nem XML + base64 SVG. É preferível não inventar
        // um valor a apresentar uma estimativa de outro backend.
        m_fileSizeLabel->setText(
            tr_(QStringLiteral("export.fileSizeUnavailable"),
                QStringLiteral("Estimate unavailable")));
        return;
    }

    const int quality = m_quality->value();
    const double scale = m_scale->currentData().toDouble();

    // Encode the small proxy thumbnail in memory, then scale the byte size
    // by pixel ratio (full_pixels / proxy_pixels).
    QByteArray buffer;
    QBuffer qb(&buffer);
    qb.open(QIODevice::WriteOnly);

    QImage proxy = m_cachedBaseThumbnail;
    // Estimate with the pixels the exporter will actually write: opaque
    // output (JPEG, or transparency disabled) is flattened onto white.
    const bool opaqueOutput = (fmt == QLatin1String("jpeg")) ||
                              !m_transparent->isChecked();
    if (opaqueOutput && proxy.hasAlphaChannel()) {
        QImage flat(proxy.size(), QImage::Format_RGB32);
        flat.fill(Qt::white);
        QPainter p(&flat);
        p.drawImage(0, 0, proxy);
        p.end();
        proxy = flat;
    }

    if (fmt == QLatin1String("jpeg")) {
        proxy.save(&qb, "JPEG", quality);
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
        m_fileSizeLabel->setText(
            tr_(QStringLiteral("export.fileSizeKB"), QStringLiteral("Est: ~%1 KB"))
                .arg(qRound(estimatedBytes / 1024.0)));
    } else {
        m_fileSizeLabel->setText(
            tr_(QStringLiteral("export.fileSizeMB"), QStringLiteral("Est: ~%1 MB"))
                .arg(estimatedBytes / (1024.0 * 1024.0), 0, 'f', 1));
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
    s.transparent = m_transparent->isChecked();
    return s;
}

void ExportDialog::retranslateUi()
{
    if (!m_i18n)
        return;

    setWindowTitle(m_i18n->t("editor", QStringLiteral("export.dialog.title")));

    if (m_scopeCaption)
        m_scopeCaption->setText(m_i18n->t("editor", QStringLiteral("export.scope")));
    if (m_presetsCaption)
        m_presetsCaption->setText(m_i18n->t("editor", QStringLiteral("export.presets")));
    if (m_outputCaption)
        m_outputCaption->setText(m_i18n->t("editor", QStringLiteral("export.output")));
    if (m_previewCaption)
        m_previewCaption->setText(m_i18n->t("editor", QStringLiteral("export.preview")));

    m_scopeEntire->setText(m_i18n->t("editor", QStringLiteral("export.scopeEntire")));
    m_scopeLayer->setText(m_i18n->t("editor", QStringLiteral("export.scopeLayer")));

    // Chips usam rótulos curtos; o tooltip carrega a descrição completa.
    const auto setChip = [this](const QString& presetId, const QString& labelKey) {
        if (auto* chip = m_presetChips.value(presetId, nullptr)) {
            chip->setText(m_i18n->t("editor", labelKey));
            chip->setToolTip(m_i18n->t("editor", presetTooltipKey(presetId)));
        }
    };
    setChip(QStringLiteral("original"), QStringLiteral("export.presetChip.original"));
    setChip(QStringLiteral("ig_post"), QStringLiteral("export.presetChip.igPost"));
    setChip(QStringLiteral("story"), QStringLiteral("export.presetChip.story"));
    setChip(QStringLiteral("yt_thumb"), QStringLiteral("export.presetChip.ytThumb"));
    setChip(QStringLiteral("x_banner"), QStringLiteral("export.presetChip.xBanner"));

    m_formatLabel->setText(m_i18n->t("editor", QStringLiteral("export.format")));
    m_qualityLabel->setText(m_i18n->t("editor", QStringLiteral("export.quality")));
    m_scaleLabel->setText(m_i18n->t("editor", QStringLiteral("export.scale")));
    m_transparentLabel->setText(
        m_i18n->t("editor", QStringLiteral("export.transparent")));

    // Icons are static; keep them attached after retranslation.
    if (auto* ig = m_presetChips.value(QStringLiteral("ig_post"), nullptr))
        ig->setIcon(QIcon(ThemeIcons::brandInstagram(16)));
    if (auto* st = m_presetChips.value(QStringLiteral("story"), nullptr))
        st->setIcon(QIcon(ThemeIcons::brandTiktok(16)));
    if (auto* yt = m_presetChips.value(QStringLiteral("yt_thumb"), nullptr))
        yt->setIcon(QIcon(ThemeIcons::brandYoutube(16)));
    if (auto* xb = m_presetChips.value(QStringLiteral("x_banner"), nullptr))
        xb->setIcon(QIcon(ThemeIcons::brandBanner(16)));

    // Formatos
    auto setComboItem = [this](QComboBox* combo, const QString& dataKey,
                               const QString& trKey) {
        if (!combo)
            return;
        for (int i = 0; i < combo->count(); ++i) {
            if (combo->itemData(i).toString() == dataKey) {
                combo->setItemText(i, m_i18n->t("editor", trKey));
                break;
            }
        }
    };
    setComboItem(m_format, QStringLiteral("png"), QStringLiteral("export.format.png"));
    setComboItem(m_format, QStringLiteral("jpeg"), QStringLiteral("export.format.jpeg"));
    setComboItem(m_format, QStringLiteral("webp"), QStringLiteral("export.format.webp"));

    // Escalas fixas (item dinâmico de preset não tem chave fixa, mantém %)
    setComboItem(m_scale, QStringLiteral("0.5"), QStringLiteral("export.scale.50"));
    setComboItem(m_scale, QStringLiteral("1"), QStringLiteral("export.scale.100"));
    setComboItem(m_scale, QStringLiteral("1.5"), QStringLiteral("export.scale.150"));
    setComboItem(m_scale, QStringLiteral("2"), QStringLiteral("export.scale.200"));

    // Recalcula preview e estimativa para aplicar textos traduzidos
    // (dimensão "%1 x %2 px" e tamanho de arquivo).
    updatePreview();
    estimateFileSize();
}

} // namespace cc