#include "imageinspector.h"

#include "core/history/CommandStack.h"
#include "core/history/DocumentCommands.h"
#include "core/image/ImageProcessing.h"
#include "localization/i18nservice.h"

#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

namespace cc {

ImageInspector::ImageInspector(I18nService* i18n, Document* document,
                               CommandStack* history, QWidget* parent)
    : QWidget(parent)
    , m_i18n(i18n)
    , m_document(document)
    , m_history(history)
{
    buildUi();

    if (m_i18n)
        connect(m_i18n, &I18nService::languageChanged,
                this, &ImageInspector::retranslateUi);
    retranslateUi();
    setVisible(false);
}

bool ImageInspector::editingLayer(ImageLayer** out) const
{
    if (!m_document || m_id.isNull())
        return false;
    Layer* layer = m_document->findLayer(m_id);
    if (!layer || layer->type() != LayerType::Image)
        return false;
    *out = static_cast<ImageLayer*>(layer);
    return true;
}

void ImageInspector::setDocument(Document* document)
{
    m_document = document;
    m_id = LayerId();
    refresh();
}

void ImageInspector::setSelectedLayer(const LayerId& id)
{
    m_id = id;
    refresh();
}

void ImageInspector::refresh()
{
    ImageLayer* layer = nullptr;
    const bool isImage = editingLayer(&layer);
    setVisible(isImage);
    if (!isImage)
        return;

    m_loading = true;
    m_tiltX->setValue(layer->transform.shearX);
    m_tiltY->setValue(layer->transform.shearY);
    resetAdjustments();
    m_loading = false;
}

void ImageInspector::resetAdjustments()
{
    m_loading = true;
    m_brightnessSlider->setValue(0);
    m_brightnessValue->setText(QStringLiteral("0%"));
    m_contrastSlider->setValue(0);
    m_contrastValue->setText(QStringLiteral("0%"));
    m_saturationSlider->setValue(0);
    m_saturationValue->setText(QStringLiteral("0%"));
    m_temperatureSlider->setValue(0);
    m_temperatureValue->setText(QStringLiteral("0%"));
    m_blurSlider->setValue(0);
    m_blurValue->setText(QStringLiteral("0px"));
    m_sharpenSlider->setValue(0);
    m_sharpenValue->setText(QStringLiteral("0%"));
    m_loading = false;
}

void ImageInspector::buildUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(8);

    // --- Grupo 1: Perspectiva 3D & Inclinação (Tilt) ---
    m_perspectiveGroup = new QGroupBox(this);
    auto* persLayout = new QFormLayout(m_perspectiveGroup);

    m_tiltX = new QDoubleSpinBox(m_perspectiveGroup);
    m_tiltX->setRange(-2.0, 2.0);
    m_tiltX->setSingleStep(0.05);
    m_tiltX->setDecimals(2);

    m_tiltY = new QDoubleSpinBox(m_perspectiveGroup);
    m_tiltY->setRange(-2.0, 2.0);
    m_tiltY->setSingleStep(0.05);
    m_tiltY->setDecimals(2);

    m_resetTiltBtn = new QPushButton(m_perspectiveGroup);

    auto* tiltRow = new QHBoxLayout;
    m_tiltXLabel = new QLabel(QStringLiteral("X:"), m_perspectiveGroup);
    tiltRow->addWidget(m_tiltXLabel);
    tiltRow->addWidget(m_tiltX);
    m_tiltYLabel = new QLabel(QStringLiteral("Y:"), m_perspectiveGroup);
    tiltRow->addWidget(m_tiltYLabel);
    tiltRow->addWidget(m_tiltY);
    tiltRow->addWidget(m_resetTiltBtn);
    persLayout->addRow(tiltRow);

    mainLayout->addWidget(m_perspectiveGroup);

    // --- Grupo 2: Ajustes Rápidos & Correção de Cor ---
    m_adjustGroup = new QGroupBox(this);
    auto* adjLayout = new QFormLayout(m_adjustGroup);

    auto makeSliderRow = [this](QLabel*& label, QSlider*& slider, QLabel*& valLabel,
                                int minVal, int maxVal, int defaultVal, const QString& unit) {
        label = new QLabel(m_adjustGroup);
        slider = new QSlider(Qt::Horizontal, m_adjustGroup);
        slider->setRange(minVal, maxVal);
        slider->setValue(defaultVal);
        valLabel = new QLabel(QStringLiteral("%1%2").arg(defaultVal).arg(unit), m_adjustGroup);
        valLabel->setFixedWidth(38);

        connect(slider, &QSlider::valueChanged, this, [valLabel, unit](int val) {
            valLabel->setText(QStringLiteral("%1%2").arg(val > 0 ? QStringLiteral("+") + QString::number(val) : QString::number(val)).arg(unit));
        });

        auto* row = new QHBoxLayout;
        row->addWidget(slider);
        row->addWidget(valLabel);
        return row;
    };

    m_brightnessLabel = new QLabel(m_adjustGroup);
    adjLayout->addRow(m_brightnessLabel, makeSliderRow(m_brightnessLabel, m_brightnessSlider, m_brightnessValue, -100, 100, 0, QStringLiteral("%")));

    m_contrastLabel = new QLabel(m_adjustGroup);
    adjLayout->addRow(m_contrastLabel, makeSliderRow(m_contrastLabel, m_contrastSlider, m_contrastValue, -100, 100, 0, QStringLiteral("%")));

    m_saturationLabel = new QLabel(m_adjustGroup);
    adjLayout->addRow(m_saturationLabel, makeSliderRow(m_saturationLabel, m_saturationSlider, m_saturationValue, -100, 100, 0, QStringLiteral("%")));

    m_temperatureLabel = new QLabel(m_adjustGroup);
    adjLayout->addRow(m_temperatureLabel, makeSliderRow(m_temperatureLabel, m_temperatureSlider, m_temperatureValue, -100, 100, 0, QStringLiteral("%")));

    m_blurLabel = new QLabel(m_adjustGroup);
    adjLayout->addRow(m_blurLabel, makeSliderRow(m_blurLabel, m_blurSlider, m_blurValue, 0, 30, 0, QStringLiteral("px")));

    m_sharpenLabel = new QLabel(m_adjustGroup);
    adjLayout->addRow(m_sharpenLabel, makeSliderRow(m_sharpenLabel, m_sharpenSlider, m_sharpenValue, 0, 100, 0, QStringLiteral("%")));

    // Botões de ação para os ajustes
    auto* btnRow = new QHBoxLayout;
    m_applyAdjustBtn = new QPushButton(m_adjustGroup);
    m_applyAdjustBtn->setStyleSheet(QStringLiteral("background-color: #2b78e4; color: white; font-weight: bold; padding: 5px; border-radius: 4px;"));
    m_resetAdjustBtn = new QPushButton(m_adjustGroup);
    btnRow->addWidget(m_applyAdjustBtn);
    btnRow->addWidget(m_resetAdjustBtn);
    adjLayout->addRow(btnRow);

    // --- Subseção: Filtros Rápidos de 1 Clique ---
    m_presetLabel = new QLabel(m_adjustGroup);
    adjLayout->addRow(m_presetLabel);

    auto* presetGrid = new QHBoxLayout;
    m_presetGrayscaleBtn = new QPushButton(QStringLiteral("P&B"), m_adjustGroup);
    m_presetSepiaBtn = new QPushButton(QStringLiteral("Sépia"), m_adjustGroup);
    m_presetVintageBtn = new QPushButton(QStringLiteral("Vintage"), m_adjustGroup);
    m_presetHighContrastBtn = new QPushButton(QStringLiteral("Contraste+"), m_adjustGroup);

    presetGrid->addWidget(m_presetGrayscaleBtn);
    presetGrid->addWidget(m_presetSepiaBtn);
    presetGrid->addWidget(m_presetVintageBtn);
    presetGrid->addWidget(m_presetHighContrastBtn);
    adjLayout->addRow(presetGrid);

    mainLayout->addWidget(m_adjustGroup);
    mainLayout->addStretch();

    // Signal connections
    connect(m_tiltX, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        ImageLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        layer->transform.shearX = val;
        if (m_document) m_document->touchLayer(m_id);
    });

    connect(m_tiltY, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        ImageLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        layer->transform.shearY = val;
        if (m_document) m_document->touchLayer(m_id);
    });

    connect(m_resetTiltBtn, &QPushButton::clicked, this, [this] {
        m_tiltX->setValue(0.0);
        m_tiltY->setValue(0.0);
    });

    connect(m_applyAdjustBtn, &QPushButton::clicked, this, &ImageInspector::applyAdjustments);
    connect(m_resetAdjustBtn, &QPushButton::clicked, this, &ImageInspector::resetAdjustments);

    connect(m_presetGrayscaleBtn, &QPushButton::clicked, this, [this] { applyPreset(0); });
    connect(m_presetSepiaBtn, &QPushButton::clicked, this, [this] { applyPreset(1); });
    connect(m_presetVintageBtn, &QPushButton::clicked, this, [this] { applyPreset(2); });
    connect(m_presetHighContrastBtn, &QPushButton::clicked, this, [this] { applyPreset(3); });
}

void ImageInspector::applyAdjustments()
{
    ImageLayer* layer = nullptr;
    if (!editingLayer(&layer) || !m_document)
        return;

    const QImage original = m_document->assets().decodedImage(layer->assetId);
    if (original.isNull())
        return;

    const double b = m_brightnessSlider->value();
    const double c = m_contrastSlider->value();
    const double s = m_saturationSlider->value();
    const double t = m_temperatureSlider->value();
    const int blur = m_blurSlider->value();
    const double sharp = m_sharpenSlider->value();

    if (b == 0 && c == 0 && s == 0 && t == 0 && blur == 0 && sharp == 0)
        return;

    QImage adjusted = original;
    if (b != 0 || c != 0 || s != 0 || t != 0) {
        adjusted = ImageProcessing::adjustColors(adjusted, b, c, s, t);
    }
    if (blur > 0) {
        adjusted = ImageProcessing::applyBlur(adjusted, blur);
    }
    if (sharp > 0) {
        adjusted = ImageProcessing::applySharpen(adjusted, sharp);
    }

    const LayerId newAssetId = m_document->assets().addImage(adjusted);
    if (m_history) {
        m_history->execute(std::make_unique<ModifyImageLayerCommand>(
            *m_document, m_id,
            layer->assetId, layer->naturalWidth, layer->naturalHeight, layer->transform,
            newAssetId, adjusted.width(), adjusted.height(), layer->transform,
            QStringLiteral("image.adjust")));
    } else {
        m_document->setImageLayerAsset(m_id, newAssetId, adjusted.width(), adjusted.height());
    }

    resetAdjustments();
}

void ImageInspector::applyPreset(int presetIndex)
{
    ImageLayer* layer = nullptr;
    if (!editingLayer(&layer) || !m_document)
        return;

    const QImage original = m_document->assets().decodedImage(layer->assetId);
    if (original.isNull())
        return;

    const auto preset = static_cast<ImageProcessing::PresetFilter>(presetIndex);
    const QImage filtered = ImageProcessing::applyPresetFilter(original, preset);

    const LayerId newAssetId = m_document->assets().addImage(filtered);
    if (m_history) {
        m_history->execute(std::make_unique<ModifyImageLayerCommand>(
            *m_document, m_id,
            layer->assetId, layer->naturalWidth, layer->naturalHeight, layer->transform,
            newAssetId, filtered.width(), filtered.height(), layer->transform,
            QStringLiteral("image.filter")));
    } else {
        m_document->setImageLayerAsset(m_id, newAssetId, filtered.width(), filtered.height());
    }
}

void ImageInspector::retranslateUi()
{
    if (m_perspectiveGroup)
        m_perspectiveGroup->setTitle(m_i18n ? m_i18n->t("editor", "image.perspective") : QStringLiteral("3D Tilt / Perspective"));
    if (m_resetTiltBtn)
        m_resetTiltBtn->setText(m_i18n ? m_i18n->t("common", "action.reset") : QStringLiteral("Reset"));

    if (m_adjustGroup)
        m_adjustGroup->setTitle(m_i18n ? m_i18n->t("editor", "image.adjustments") : QStringLiteral("Color Adjustments & Filters"));
    if (m_brightnessLabel)
        m_brightnessLabel->setText(m_i18n ? m_i18n->t("editor", "image.brightness") : QStringLiteral("Brightness:"));
    if (m_contrastLabel)
        m_contrastLabel->setText(m_i18n ? m_i18n->t("editor", "image.contrast") : QStringLiteral("Contrast:"));
    if (m_saturationLabel)
        m_saturationLabel->setText(m_i18n ? m_i18n->t("editor", "image.saturation") : QStringLiteral("Saturation:"));
    if (m_temperatureLabel)
        m_temperatureLabel->setText(m_i18n ? m_i18n->t("editor", "image.temperature") : QStringLiteral("Temperature:"));
    if (m_blurLabel)
        m_blurLabel->setText(m_i18n ? m_i18n->t("editor", "image.blur") : QStringLiteral("Blur:"));
    if (m_sharpenLabel)
        m_sharpenLabel->setText(m_i18n ? m_i18n->t("editor", "image.sharpen") : QStringLiteral("Sharpen:"));

    if (m_applyAdjustBtn)
        m_applyAdjustBtn->setText(m_i18n ? m_i18n->t("common", "action.apply") : QStringLiteral("Apply Adjustments"));
    if (m_resetAdjustBtn)
        m_resetAdjustBtn->setText(m_i18n ? m_i18n->t("common", "action.reset") : QStringLiteral("Reset"));

    if (m_presetLabel)
        m_presetLabel->setText(m_i18n ? m_i18n->t("editor", "image.presets") : QStringLiteral("Quick 1-Click Filters:"));
}

} // namespace cc
