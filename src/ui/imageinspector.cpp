#include "imageinspector.h"
#include "collapsiblesection.h"
#include "core/history/CommandStack.h"
#include "core/history/DocumentCommands.h"
#include "core/image/ImageProcessing.h"
#include "localization/i18nservice.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QSlider>
#include <QVBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <future>
#include <thread>
#include <QMetaObject>

namespace cc {

ImageInspector::ImageInspector(I18nService* i18n, Document* document,
                               CommandStack* history, QWidget* parent)
    : QWidget(parent)
    , m_i18n(i18n)
    , m_document(document)
    , m_history(history)
{
    m_previewTimer.setSingleShot(true);
    connect(&m_previewTimer, &QTimer::timeout, this, &ImageInspector::onPreviewTimeout);

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
    m_originalAssetId = LayerId();
    m_currentPreviewAssetId = LayerId();
    m_originalPreviewImage = QImage();
    refresh();
}

void ImageInspector::setSelectedLayer(const LayerId& id)
{
    // Se havia preview pendente não aplicada na camada anterior, restaura o asset original
    ImageLayer* prevLayer = nullptr;
    if (editingLayer(&prevLayer) && !m_currentPreviewAssetId.isNull() && !m_originalAssetId.isNull()) {
        if (prevLayer->assetId != m_originalAssetId) {
            const QImage orig = m_document->assets().decodedImage(m_originalAssetId);
            if (!orig.isNull()) {
                m_document->setImageLayerAsset(m_id, m_originalAssetId, orig.width(), orig.height());
                m_document->touchLayer(m_id);
            }
        }
    }

    m_previewTimer.stop();
    m_id = id;
    m_originalAssetId = LayerId();
    m_currentPreviewAssetId = LayerId();

    ImageLayer* newLayer = nullptr;
    if (editingLayer(&newLayer)) {
        m_originalAssetId = newLayer->assetId;
        m_originalPreviewImage = m_document->assets().decodedImage(m_originalAssetId);
    }

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
    m_originalAssetId = layer->assetId;
    m_currentPreviewAssetId = LayerId();
    m_originalPreviewImage = m_document->assets().decodedImage(m_originalAssetId);
    m_tiltX->setValue(layer->transform.shearX);
    m_tiltY->setValue(layer->transform.shearY);

    if (m_outlineOn)
        m_outlineOn->setChecked(layer->effects.outline.enabled);
    if (m_outlineWidth)
        m_outlineWidth->setValue(layer->effects.outline.width);
    if (m_outlineBlur)
        m_outlineBlur->setValue(layer->effects.outline.blur);
    if (m_outlineColor) {
        const QString hex = layer->effects.outline.color.name(QColor::HexRgb);
        m_outlineColor->setText(hex);
        m_outlineColor->setStyleSheet(QStringLiteral("background: %1; color: %2; font-weight: bold; border-radius: 4px; padding: 2px 6px;")
            .arg(hex, layer->effects.outline.color.lightness() > 128 ? QStringLiteral("#000000") : QStringLiteral("#ffffff")));
    }

    resetAdjustments();
    m_loading = false;
}

void ImageInspector::resetAdjustments()
{
    m_previewTimer.stop();
    m_loading = true;
    m_brightnessSlider->setValue(0);
    m_exposureSlider->setValue(0);
    m_brightnessValue->setText(QStringLiteral("0%"));
    m_contrastSlider->setValue(0);
    m_contrastValue->setText(QStringLiteral("0%"));
    m_saturationSlider->setValue(0);
    m_saturationValue->setText(QStringLiteral("0%"));
    m_temperatureSlider->setValue(0);
    m_temperatureValue->setText(QStringLiteral("0%"));
    m_hueSlider->setValue(0);
    m_hueValue->setText(QStringLiteral("0°"));
    m_blurSlider->setValue(0);
    m_blurValue->setText(QStringLiteral("0px"));
    m_sharpenSlider->setValue(0);
    m_shadowsSlider->setValue(0);
    m_highlightsSlider->setValue(0);
    m_vignetteSlider->setValue(0);
    m_sharpenValue->setText(QStringLiteral("0%"));
    m_loading = false;

    // Restaura o asset original no canvas
    ImageLayer* layer = nullptr;
    if (editingLayer(&layer) && m_document && !m_originalAssetId.isNull()) {
        if (layer->assetId != m_originalAssetId) {
            const QImage orig = m_document->assets().decodedImage(m_originalAssetId);
            if (!orig.isNull()) {
                m_document->setImageLayerAsset(m_id, m_originalAssetId, orig.width(), orig.height());
                m_document->touchLayer(m_id);
            }
        }
    }
    m_currentPreviewAssetId = LayerId();
}

void ImageInspector::buildUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(2, 2, 2, 2);
    mainLayout->setSpacing(4);

    // --- 1. Filtros Rápidos (Aberto por padrão) ---
    m_quickFiltersSection = new CollapsibleSection(QStringLiteral("Quick 1-Click Filters"), true, this);
    auto* quickLayout = new QVBoxLayout;
    quickLayout->setContentsMargins(4, 4, 4, 4);

    m_presetLabel = new QLabel(this);
    quickLayout->addWidget(m_presetLabel);

    auto* presetGrid = new QHBoxLayout;
    m_presetGrayscaleBtn = new QPushButton(QStringLiteral("P&B"), this);
    m_presetSepiaBtn = new QPushButton(QStringLiteral("Sépia"), this);
    m_presetVintageBtn = new QPushButton(QStringLiteral("Vintage"), this);
    m_presetHighContrastBtn = new QPushButton(QStringLiteral("Contraste+"), this);

    presetGrid->addWidget(m_presetGrayscaleBtn);
    presetGrid->addWidget(m_presetSepiaBtn);
    presetGrid->addWidget(m_presetVintageBtn);
    presetGrid->addWidget(m_presetHighContrastBtn);
    quickLayout->addLayout(presetGrid);

    auto* presetGrid2 = new QHBoxLayout;
    m_presetVividPopBtn = new QPushButton(QStringLiteral("Vivid Pop"), this);
    m_presetCyberpunkBtn = new QPushButton(QStringLiteral("Cyberpunk"), this);
    m_presetGoldenHourBtn = new QPushButton(QStringLiteral("Golden Hour"), this);
    m_presetNoirBtn = new QPushButton(QStringLiteral("Noir"), this);

    presetGrid2->addWidget(m_presetVividPopBtn);
    presetGrid2->addWidget(m_presetCyberpunkBtn);
    presetGrid2->addWidget(m_presetGoldenHourBtn);
    presetGrid2->addWidget(m_presetNoirBtn);
    quickLayout->addLayout(presetGrid2);

    m_quickFiltersSection->setContentLayout(quickLayout);
    mainLayout->addWidget(m_quickFiltersSection);

    // --- 2. Contorno & Glow / Sticker Effect (Aberto por padrão para thumbnails) ---
    m_outlineSection = new CollapsibleSection(QStringLiteral("Contorno & Glow (Sticker)"), true, this);
    auto* fxLayout = new QVBoxLayout;
    fxLayout->setContentsMargins(4, 4, 4, 4);
    fxLayout->setSpacing(6);

    auto* enableRow = new QHBoxLayout;
    m_outlineOn = new QCheckBox(QStringLiteral("Ativar Borda / Brilho"), this);
    m_outlineColor = new QPushButton(this);
    m_outlineColor->setFixedWidth(75);
    enableRow->addWidget(m_outlineOn);
    enableRow->addWidget(m_outlineColor);
    enableRow->addStretch();
    fxLayout->addLayout(enableRow);

    // Paleta de cores rápidas para stickers de thumbnail (Branco, Amarelo Neon, Ciano Neon, Rosa/Vermelho)
    auto* quickColorsRow = new QHBoxLayout;
    const QVector<QPair<QString, QColor>> quickColors = {
        {QStringLiteral("Branco"), QColor(255, 255, 255)},
        {QStringLiteral("Amarelo"), QColor(255, 230, 0)},
        {QStringLiteral("Ciano"), QColor(0, 240, 255)},
        {QStringLiteral("Neon"), QColor(57, 255, 20)},
        {QStringLiteral("Vermelho"), QColor(255, 40, 40)}
    };
    for (const auto& qc : quickColors) {
        auto* qBtn = new QPushButton(qc.first, this);
        qBtn->setStyleSheet(QStringLiteral("background: %1; color: %2; font-size: 10px; font-weight: bold; border-radius: 3px; padding: 2px 4px;")
            .arg(qc.second.name(), qc.second.lightness() > 128 ? QStringLiteral("#000000") : QStringLiteral("#ffffff")));
        connect(qBtn, &QPushButton::clicked, this, [this, color = qc.second] {
            ImageLayer* layer = nullptr;
            if (!editingLayer(&layer)) return;
            layer->effects.outline.color = color;
            if (m_outlineColor) {
                const QString hex = color.name(QColor::HexRgb);
                m_outlineColor->setText(hex);
                m_outlineColor->setStyleSheet(QStringLiteral("background: %1; color: %2; font-weight: bold; border-radius: 4px;")
                    .arg(hex, color.lightness() > 128 ? QStringLiteral("#000000") : QStringLiteral("#ffffff")));
            }
            if (m_document) m_document->touchLayer(m_id);
        });
        quickColorsRow->addWidget(qBtn);
    }
    fxLayout->addLayout(quickColorsRow);

    auto* fxForm = new QFormLayout;
    fxForm->setContentsMargins(0, 0, 0, 0);

    m_outlineWidthLabel = new QLabel(QStringLiteral("Espessura:"), this);
    m_outlineWidth = new QDoubleSpinBox(this);
    m_outlineWidth->setRange(1.0, 50.0);
    m_outlineWidth->setValue(8.0);
    m_outlineWidth->setSuffix(QStringLiteral(" px"));
    fxForm->addRow(m_outlineWidthLabel, m_outlineWidth);

    m_outlineBlurLabel = new QLabel(QStringLiteral("Desfoque / Glow:"), this);
    m_outlineBlur = new QDoubleSpinBox(this);
    m_outlineBlur->setRange(0.0, 40.0);
    m_outlineBlur->setValue(0.0);
    m_outlineBlur->setSuffix(QStringLiteral(" px"));
    fxForm->addRow(m_outlineBlurLabel, m_outlineBlur);

    fxLayout->addLayout(fxForm);
    m_outlineSection->setContentLayout(fxLayout);
    mainLayout->addWidget(m_outlineSection);

    // --- 3. Ajustes Manuais de Cor e Filtros (Recolhido por padrão) ---
    m_adjustmentsSection = new CollapsibleSection(QStringLiteral("Color Adjustments & Filters"), false, this);
    auto* adjLayout = new QFormLayout;
    adjLayout->setContentsMargins(4, 4, 4, 4);

    auto makeSliderRow = [this](QLabel*& label, QSlider*& slider, QLabel*& valLabel,
                                int minVal, int maxVal, int defaultVal, const QString& unit) {
        label = new QLabel(this);
        slider = new QSlider(Qt::Horizontal, this);
        slider->setRange(minVal, maxVal);
        slider->setValue(defaultVal);
        valLabel = new QLabel(QStringLiteral("%1%2").arg(defaultVal).arg(unit), this);
        valLabel->setFixedWidth(38);

        connect(slider, &QSlider::valueChanged, this, [this, valLabel, unit](int val) {
            valLabel->setText(QStringLiteral("%1%2").arg(val > 0 ? QStringLiteral("+") + QString::number(val) : QString::number(val)).arg(unit));
            if (!m_loading)
                schedulePreview();
        });

        auto* row = new QHBoxLayout;
        row->addWidget(slider);
        row->addWidget(valLabel);
        return row;
    };

    m_brightnessLabel = new QLabel(this);
    adjLayout->addRow(m_brightnessLabel, makeSliderRow(m_brightnessLabel, m_brightnessSlider, m_brightnessValue, -100, 100, 0, QStringLiteral("%")));

    m_exposureLabel = new QLabel(this);
    adjLayout->addRow(m_exposureLabel, makeSliderRow(m_exposureLabel, m_exposureSlider, m_exposureValue, -100, 100, 0, QStringLiteral("%")));

    m_contrastLabel = new QLabel(this);
    adjLayout->addRow(m_contrastLabel, makeSliderRow(m_contrastLabel, m_contrastSlider, m_contrastValue, -100, 100, 0, QStringLiteral("%")));

    m_saturationLabel = new QLabel(this);
    adjLayout->addRow(m_saturationLabel, makeSliderRow(m_saturationLabel, m_saturationSlider, m_saturationValue, -100, 100, 0, QStringLiteral("%")));

    m_temperatureLabel = new QLabel(this);
    adjLayout->addRow(m_temperatureLabel, makeSliderRow(m_temperatureLabel, m_temperatureSlider, m_temperatureValue, -100, 100, 0, QStringLiteral("%")));

    m_hueLabel = new QLabel(this);
    adjLayout->addRow(m_hueLabel, makeSliderRow(m_hueLabel, m_hueSlider, m_hueValue, -180, 180, 0, QStringLiteral("°")));

    m_blurLabel = new QLabel(this);
    adjLayout->addRow(m_blurLabel, makeSliderRow(m_blurLabel, m_blurSlider, m_blurValue, 0, 30, 0, QStringLiteral("px")));

    m_sharpenLabel = new QLabel(this);
    adjLayout->addRow(m_sharpenLabel, makeSliderRow(m_sharpenLabel, m_sharpenSlider, m_sharpenValue, 0, 100, 0, QStringLiteral("%")));
    m_shadowsLabel = new QLabel(this);
    adjLayout->addRow(m_shadowsLabel, makeSliderRow(m_shadowsLabel, m_shadowsSlider, m_shadowsValue, -100, 100, 0, QStringLiteral("%")));
    m_highlightsLabel = new QLabel(this);
    adjLayout->addRow(m_highlightsLabel, makeSliderRow(m_highlightsLabel, m_highlightsSlider, m_highlightsValue, -100, 100, 0, QStringLiteral("%")));
    m_vignetteLabel = new QLabel(this);
    adjLayout->addRow(m_vignetteLabel, makeSliderRow(m_vignetteLabel, m_vignetteSlider, m_vignetteValue, 0, 100, 0, QStringLiteral("%")));

    auto* btnRow = new QHBoxLayout;
    m_applyAdjustBtn = new QPushButton(this);
    m_applyAdjustBtn->setStyleSheet(QStringLiteral("background-color: #2b78e4; color: white; font-weight: bold; padding: 5px; border-radius: 4px;"));
    m_resetAdjustBtn = new QPushButton(this);
    btnRow->addWidget(m_applyAdjustBtn);
    btnRow->addWidget(m_resetAdjustBtn);
    adjLayout->addRow(btnRow);

    m_adjustmentsSection->setContentLayout(adjLayout);
    mainLayout->addWidget(m_adjustmentsSection);

    // --- 3. Perspectiva 3D & Inclinação (Recolhido por padrão) ---
    m_perspectiveSection = new CollapsibleSection(QStringLiteral("3D Tilt & Perspective"), false, this);
    auto* persLayout = new QFormLayout;
    persLayout->setContentsMargins(4, 4, 4, 4);

    m_tiltX = new QDoubleSpinBox(this);
    m_tiltX->setRange(-2.0, 2.0);
    m_tiltX->setSingleStep(0.05);
    m_tiltX->setDecimals(2);

    m_tiltY = new QDoubleSpinBox(this);
    m_tiltY->setRange(-2.0, 2.0);
    m_tiltY->setSingleStep(0.05);
    m_tiltY->setDecimals(2);

    m_resetTiltBtn = new QPushButton(this);

    auto* tiltRow = new QHBoxLayout;
    m_tiltXLabel = new QLabel(QStringLiteral("X:"), this);
    tiltRow->addWidget(m_tiltXLabel);
    tiltRow->addWidget(m_tiltX);
    m_tiltYLabel = new QLabel(QStringLiteral("Y:"), this);
    tiltRow->addWidget(m_tiltYLabel);
    tiltRow->addWidget(m_tiltY);
    tiltRow->addWidget(m_resetTiltBtn);
    persLayout->addRow(tiltRow);

    m_perspectiveSection->setContentLayout(persLayout);
    mainLayout->addWidget(m_perspectiveSection);

    // --- Mockup / Logo (warped, não destrutivo e com undo) ---
    m_mockupSection = new CollapsibleSection(QStringLiteral("Mockup / Substituir Logo"), false, this);
    auto* mockupLayout = new QVBoxLayout;
    mockupLayout->setContentsMargins(4, 4, 4, 4);
    m_mockupHint = new QLabel(QStringLiteral("Importe uma logo e marque uma área livre sobre a imagem. Clique duas vezes ou pressione Enter para confirmar."), this);
    m_mockupHint->setWordWrap(true);
    m_mockupHint->setStyleSheet(QStringLiteral("color: #8f96a3;"));
    mockupLayout->addWidget(m_mockupHint);
    m_mockupButton = new QPushButton(QStringLiteral("Importar logo e criar mockup"), this);
    m_mockupButton->setStyleSheet(QStringLiteral("background:#2b78e4;color:white;font-weight:bold;padding:6px;border-radius:4px;"));
    mockupLayout->addWidget(m_mockupButton);
    m_mockupSection->setContentLayout(mockupLayout);
    mainLayout->addWidget(m_mockupSection);

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
    connect(m_presetVividPopBtn, &QPushButton::clicked, this, [this] { applyPreset(4); });
    connect(m_presetCyberpunkBtn, &QPushButton::clicked, this, [this] { applyPreset(5); });
    connect(m_presetGoldenHourBtn, &QPushButton::clicked, this, [this] { applyPreset(6); });
    connect(m_presetNoirBtn, &QPushButton::clicked, this, [this] { applyPreset(7); });

    // Conexões de Contorno / Glow (Sticker)
    connect(m_outlineOn, &QCheckBox::toggled, this, [this](bool on) {
        ImageLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        layer->effects.outline.enabled = on;
        if (m_document) m_document->touchLayer(m_id);
    });

    connect(m_outlineColor, &QPushButton::clicked, this, [this] {
        ImageLayer* layer = nullptr;
        if (!editingLayer(&layer)) return;
        const QColor chosen = QColorDialog::getColor(layer->effects.outline.color, this);
        if (!chosen.isValid()) return;
        layer->effects.outline.color = chosen;
        const QString hex = chosen.name(QColor::HexRgb);
        m_outlineColor->setText(hex);
        m_outlineColor->setStyleSheet(QStringLiteral("background: %1; color: %2; font-weight: bold; border-radius: 4px;")
            .arg(hex, chosen.lightness() > 128 ? QStringLiteral("#000000") : QStringLiteral("#ffffff")));
        if (m_document) m_document->touchLayer(m_id);
    });

    connect(m_outlineWidth, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        ImageLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        layer->effects.outline.width = val;
        if (m_document) m_document->touchLayer(m_id);
    });

    connect(m_outlineBlur, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        ImageLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        layer->effects.outline.blur = val;
        if (m_document) m_document->touchLayer(m_id);
    });

    connect(m_mockupButton, &QPushButton::clicked, this, &ImageInspector::openMockupDialog);
}

void ImageInspector::openMockupDialog()
{
    ImageLayer* selected = nullptr;
    if (!m_document || !editingLayer(&selected)) return;

    QDialog sourceDialog(this);
    sourceDialog.setWindowTitle(QStringLiteral("Escolher imagem para o mockup"));
    auto* sourceForm = new QFormLayout(&sourceDialog);
    auto* sourceCombo = new QComboBox(&sourceDialog);
    sourceCombo->addItem(QStringLiteral("Importar uma imagem..."), QString());
    for (const auto& child : m_document->rootGroup()->children) {
        if (!child || child->type() != LayerType::Image || child->id() == m_id)
            continue;
        const auto* imageLayer = static_cast<const ImageLayer*>(child.get());
        sourceCombo->addItem(imageLayer->name.isEmpty() ? QStringLiteral("Imagem existente") : imageLayer->name,
                             imageLayer->id().toString(QUuid::WithoutBraces));
    }
    sourceForm->addRow(QStringLiteral("Imagem fonte"), sourceCombo);
    auto* removeBackground = new QCheckBox(QStringLiteral("Remover fundo uniforme (Varinha Mágica)"), &sourceDialog);
    removeBackground->setToolTip(QStringLiteral("Útil para JPEGs com fundo branco ou de cor uniforme. O ficheiro original não é alterado."));
    sourceForm->addRow(removeBackground);
    auto* sourceButtons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &sourceDialog);
    sourceForm->addRow(sourceButtons);
    connect(sourceButtons, &QDialogButtonBox::accepted, &sourceDialog, &QDialog::accept);
    connect(sourceButtons, &QDialogButtonBox::rejected, &sourceDialog, &QDialog::reject);
    if (sourceDialog.exec() != QDialog::Accepted)
        return;

    QImage logo;
    const QString existingId = sourceCombo->currentData().toString();
    if (!existingId.isEmpty()) {
        const LayerId sourceId(existingId);
        if (auto* sourceLayer = m_document->findLayer(sourceId); sourceLayer && sourceLayer->type() == LayerType::Image)
            logo = m_document->assets().decodedImage(static_cast<ImageLayer*>(sourceLayer)->assetId);
    } else {
        const QString logoPath = QFileDialog::getOpenFileName(this, QStringLiteral("Escolher logo"), QString(),
            QStringLiteral("Imagens (*.png *.jpg *.jpeg *.webp *.bmp)"));
        if (!logoPath.isEmpty())
            logo = QImage(logoPath);
    }
    if (logo.isNull()) return;

    if (removeBackground->isChecked() && !logo.isNull()) {
        // Usa um canto como semente, exatamente como a Varinha Mágica.
        logo = ImageProcessing::removeBackground(logo, QPoint(0, 0), 18, true);
    }

    m_pendingMockupLogo = logo;
    if (m_mockupHint)
        m_mockupHint->setText(QStringLiteral("Agora marque, na imagem, a área onde a fonte deve ficar. Duplo clique ou Enter confirma."));
    emit mockupPlacementRequested();
}

void ImageInspector::setMockupArea(const QPolygonF& destination)
{
    if (!m_document || m_pendingMockupLogo.isNull() || destination.size() < 3)
        return;
    QImage logo = m_pendingMockupLogo;
    m_pendingMockupLogo = QImage();

    const QVector<QPair<QString, BlendMode>> modes = {{QStringLiteral("Normal"), BlendMode::Normal},
        {QStringLiteral("Multiply"), BlendMode::Multiply}, {QStringLiteral("Screen"), BlendMode::Screen},
        {QStringLiteral("Overlay"), BlendMode::Overlay}, {QStringLiteral("Darken"), BlendMode::Darken},
        {QStringLiteral("Lighten"), BlendMode::Lighten}, {QStringLiteral("Add"), BlendMode::Add}};
    const QRectF targetBounds = destination.boundingRect();
    const QPolygonF quadDestination = destination.size() == 4
        ? destination
        : QPolygonF{targetBounds.topLeft(), targetBounds.topRight(),
                    targetBounds.bottomRight(), targetBounds.bottomLeft()};
    const QPolygonF source{QPointF(0, 0), QPointF(logo.width(), 0),
                           QPointF(logo.width(), logo.height()), QPointF(0, logo.height())};
    QTransform warp;
    if (!QTransform::quadToQuad(source, quadDestination, warp)) return;

    QImage canvas(m_document->width(), m_document->height(), QImage::Format_ARGB32_Premultiplied);
    canvas.fill(Qt::transparent);
    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    // A área marcada define a deformação, não uma máscara de corte. A fonte
    // inteira é projetada para o quadrilátero e o alfa original do PNG é
    // preservado sem eliminar pixels fora do polígono livre.
    painter.setTransform(warp);
    painter.drawImage(QPointF(0, 0), logo);
    painter.end();

    // Mantém a imagem inteira deformada: apenas recorta o espaço de trabalho
    // da área marcada, nunca a caixa alfa dos pixels da fonte. Assim nenhum
    // pixel visível do PNG é eliminado; as zonas transparentes continuam
    // transparentes dentro do mockup.
    // Recorta apenas o bounding box dos pixels realmente visíveis. Nunca
    // usamos a caixa da seleção como limite, pois isso elimina partes da
    // imagem deformada quando a fonte ultrapassa ligeiramente o quadrilátero.
    QRect contentRect;
    int left = canvas.width(), top = canvas.height(), right = -1, bottom = -1;
    for (int y = 0; y < canvas.height(); ++y) {
        const QRgb* line = reinterpret_cast<const QRgb*>(canvas.constScanLine(y));
        for (int x = 0; x < canvas.width(); ++x) {
            if (qAlpha(line[x]) == 0) continue;
            left = std::min(left, x); top = std::min(top, y);
            right = std::max(right, x); bottom = std::max(bottom, y);
        }
    }
    if (right >= left && bottom >= top)
        contentRect = QRect(QPoint(left, top), QPoint(right, bottom)).intersected(canvas.rect());
    if (contentRect.isEmpty())
        return;
    const QImage cropped = canvas.copy(contentRect);

    auto overlay = std::make_unique<ImageLayer>();
    overlay->name = QStringLiteral("Mockup — Logo");
    const LayerId croppedAsset = m_document->assets().addImage(cropped);
    overlay->assetId = croppedAsset;
    overlay->naturalWidth = cropped.width();
    overlay->naturalHeight = cropped.height();
    // AffineTransform usa o centro do conteúdo como origem visual.
    overlay->transform.position = contentRect.topLeft()
        + QPointF(cropped.width() / 2.0, cropped.height() / 2.0);

    const LayerId previewId = overlay->id();
    // Adiciona temporariamente a camada para que o diálogo mostre o resultado.
    m_document->addLayer(std::move(overlay));
    auto* previewLayer = static_cast<ImageLayer*>(m_document->findLayer(previewId));
    if (!previewLayer) return;
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Pré-visualização — Mockup / Substituir Logo"));
    auto* form = new QFormLayout(&dialog);
    form->addRow(new QLabel(QStringLiteral("Ajuste o resultado no canvas. OK confirma; Cancelar ou Esc remove a prévia."), &dialog));
    auto* opacity = new QSlider(Qt::Horizontal, &dialog); opacity->setRange(1, 100); opacity->setValue(100);
    form->addRow(QStringLiteral("Opacidade"), opacity);
    auto* blend = new QComboBox(&dialog);
    for (const auto& mode : modes) blend->addItem(mode.first, static_cast<int>(mode.second));
    form->addRow(QStringLiteral("Mistura"), blend);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    connect(opacity, &QSlider::valueChanged, &dialog, [this, previewId](int value) {
        if (auto* layer = m_document ? m_document->findLayer(previewId) : nullptr) {
            layer->setOpacity(value / 100.0f);
            m_document->touchLayer(previewId);
        }
    });
    connect(blend, qOverload<int>(&QComboBox::currentIndexChanged), &dialog, [this, previewId, blend](int) {
        if (auto* layer = m_document ? m_document->findLayer(previewId) : nullptr) {
            layer->blendMode = static_cast<BlendMode>(blend->currentData().toInt());
            m_document->touchLayer(previewId);
        }
    });
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    const bool accepted = dialog.exec() == QDialog::Accepted;
    auto finalLayer = m_document->takeLayer(previewId);
    if (!finalLayer) return;
    if (accepted && m_history)
        m_history->execute(std::make_unique<AddLayerCommand>(*m_document, std::move(finalLayer)));
    else if (accepted)
        m_document->addLayer(std::move(finalLayer));
    if (m_mockupHint)
        m_mockupHint->setText(accepted
            ? QStringLiteral("Mockup criado numa camada separada. Pode selecionar, mover, ocultar ou desfazer.")
            : QStringLiteral("Pré-visualização cancelada."));
}

void ImageInspector::schedulePreview()
{
    // Evita recalcular a imagem a cada evento individual do slider.
    // O último valor é aplicado quando o utilizador abranda ou para o arrasto.
    m_previewTimer.start(100);
}

void ImageInspector::onPreviewTimeout()
{
    ImageLayer* layer = nullptr;
    if (!editingLayer(&layer) || !m_document || m_originalAssetId.isNull())
        return;

    const int b = m_brightnessSlider->value();
    const int exposure = m_exposureSlider->value();
    const int c = m_contrastSlider->value();
    const int s = m_saturationSlider->value();
    const int t = m_temperatureSlider->value();
    const int h = m_hueSlider->value();
    const int blur = m_blurSlider->value();
    const int sharp = m_sharpenSlider->value();
    const int shadows = m_shadowsSlider->value();
    const int highlights = m_highlightsSlider->value();
    const int vignette = m_vignetteSlider->value();

    const QImage original = m_originalPreviewImage.isNull()
        ? m_document->assets().decodedImage(m_originalAssetId)
        : m_originalPreviewImage;
    if (original.isNull())
        return;

    if (b == 0 && exposure == 0 && c == 0 && s == 0 && t == 0 && h == 0 && blur == 0 && sharp == 0 && shadows == 0 && highlights == 0 && vignette == 0) {
        if (layer->assetId != m_originalAssetId) {
            m_document->setImageLayerAsset(m_id, m_originalAssetId, original.width(), original.height());
            m_document->touchLayer(m_id);
            m_currentPreviewAssetId = LayerId();
        }
        return;
    }

    // Processa uma versão reduzida durante o preview para manter os sliders
    // responsivos. O resultado volta ao tamanho original antes de ser exibido.
    constexpr qint64 kPreviewPixels = 1600000;
    const bool reducedPreview = qint64(original.width()) * original.height() > kPreviewPixels;
    const QImage previewSource = reducedPreview
        ? original.scaled(QSize(1400, 1400), Qt::KeepAspectRatio, Qt::SmoothTransformation)
        : original;
    const unsigned generation = ++m_previewGeneration;
    const LayerId layerId = m_id;
    const QImage source = previewSource;
    auto future = std::async(std::launch::async, [source, b, exposure, c, s, t, h, blur, sharp, shadows, highlights, vignette,
                                                   reducedPreview, originalSize = original.size()] {
        QImage adjusted = source;
        if (b != 0 || exposure != 0 || c != 0 || s != 0 || t != 0 || h != 0 || shadows != 0 || highlights != 0 || vignette != 0)
            adjusted = ImageProcessing::adjustColors(adjusted, b + exposure, c, s, t, h, shadows, highlights, vignette);
        if (blur > 0) adjusted = ImageProcessing::applyBlur(adjusted, blur);
        if (sharp > 0) adjusted = ImageProcessing::applySharpen(adjusted, sharp);
        if (reducedPreview && adjusted.size() != originalSize)
            adjusted = adjusted.scaled(originalSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        return adjusted;
    });
    std::thread([this, generation, layerId, future = std::move(future)]() mutable {
        QImage adjusted = future.get();
        QMetaObject::invokeMethod(this, [this, generation, layerId, adjusted = std::move(adjusted)]() mutable {
            if (generation != m_previewGeneration.load() || layerId != m_id || !m_document)
                return;
            m_latestPreviewImage = adjusted;
            emit imagePreviewReady(layerId, adjusted);
        }, Qt::QueuedConnection);
    }).detach();
}

void ImageInspector::applyAdjustments()
{
    m_previewTimer.stop();
    ImageLayer* layer = nullptr;
    if (!editingLayer(&layer) || !m_document || m_originalAssetId.isNull())
        return;

    if (m_latestPreviewImage.isNull() && (m_currentPreviewAssetId.isNull() || m_currentPreviewAssetId == m_originalAssetId))
        return;

    if (m_currentPreviewAssetId.isNull() && !m_latestPreviewImage.isNull())
        m_currentPreviewAssetId = m_document->assets().addImage(m_latestPreviewImage);

    const QImage finalImg = m_latestPreviewImage.isNull()
        ? m_document->assets().decodedImage(m_currentPreviewAssetId)
        : m_latestPreviewImage;
    if (finalImg.isNull())
        return;

    const LayerId finalAssetId = m_currentPreviewAssetId;
    const LayerId origAssetId = m_originalAssetId;

    // Restaura temporariamente para origAssetId para que o comando execute a transição correta
    m_document->setImageLayerAsset(m_id, origAssetId, layer->naturalWidth, layer->naturalHeight);

    if (m_history) {
        m_history->execute(std::make_unique<ModifyImageLayerCommand>(
            *m_document, m_id,
            origAssetId, layer->naturalWidth, layer->naturalHeight, layer->transform,
            finalAssetId, finalImg.width(), finalImg.height(), layer->transform,
            QStringLiteral("image.adjust")));
    } else {
        m_document->setImageLayerAsset(m_id, finalAssetId, finalImg.width(), finalImg.height());
        m_document->touchLayer(m_id);
    }

    m_originalAssetId = finalAssetId;
    m_currentPreviewAssetId = LayerId();
    m_latestPreviewImage = QImage();
    emit imagePreviewCleared();

    m_loading = true;
    m_brightnessSlider->setValue(0);
    m_exposureSlider->setValue(0);
    m_brightnessValue->setText(QStringLiteral("0%"));
    m_contrastSlider->setValue(0);
    m_contrastValue->setText(QStringLiteral("0%"));
    m_saturationSlider->setValue(0);
    m_saturationValue->setText(QStringLiteral("0%"));
    m_temperatureSlider->setValue(0);
    m_temperatureValue->setText(QStringLiteral("0%"));
    m_hueSlider->setValue(0);
    m_hueValue->setText(QStringLiteral("0°"));
    m_blurSlider->setValue(0);
    m_blurValue->setText(QStringLiteral("0px"));
    m_sharpenSlider->setValue(0);
    m_sharpenValue->setText(QStringLiteral("0%"));
    m_loading = false;
}

void ImageInspector::applyPreset(int presetIndex)
{
    m_loading = true;
    m_brightnessSlider->setValue(0);
    m_exposureSlider->setValue(0);
    m_contrastSlider->setValue(0);
    m_saturationSlider->setValue(0);
    m_temperatureSlider->setValue(0);
    m_hueSlider->setValue(0);
    m_blurSlider->setValue(0);
    m_sharpenSlider->setValue(0);

    switch (presetIndex) {
    case 0: // P&B (Grayscale)
        m_saturationSlider->setValue(-100);
        m_contrastSlider->setValue(10);
        break;
    case 1: // Sépia
        m_saturationSlider->setValue(-50);
        m_temperatureSlider->setValue(40);
        m_contrastSlider->setValue(5);
        break;
    case 2: // Vintage
        m_saturationSlider->setValue(-25);
        m_temperatureSlider->setValue(20);
        m_contrastSlider->setValue(15);
        break;
    case 3: // Alto Contraste
        m_contrastSlider->setValue(50);
        m_sharpenSlider->setValue(20);
        break;
    case 4: // Vivid Pop
        m_saturationSlider->setValue(45);
        m_contrastSlider->setValue(25);
        m_sharpenSlider->setValue(15);
        break;
    case 5: // Cyberpunk
        m_contrastSlider->setValue(35);
        m_saturationSlider->setValue(30);
        m_temperatureSlider->setValue(-40);
        m_sharpenSlider->setValue(10);
        break;
    case 6: // Golden Hour
        m_temperatureSlider->setValue(45);
        m_saturationSlider->setValue(20);
        m_brightnessSlider->setValue(10);
        m_contrastSlider->setValue(10);
        break;
    case 7: // Dramatic Noir
        m_saturationSlider->setValue(-100);
        m_contrastSlider->setValue(60);
        m_brightnessSlider->setValue(-10);
        m_sharpenSlider->setValue(25);
        break;
    }

    auto formatVal = [](int val, const QString& unit) {
        return QStringLiteral("%1%2").arg(val > 0 ? QStringLiteral("+") + QString::number(val) : QString::number(val)).arg(unit);
    };

    m_brightnessValue->setText(formatVal(m_brightnessSlider->value(), QStringLiteral("%")));
    m_contrastValue->setText(formatVal(m_contrastSlider->value(), QStringLiteral("%")));
    m_saturationValue->setText(formatVal(m_saturationSlider->value(), QStringLiteral("%")));
    m_temperatureValue->setText(formatVal(m_temperatureSlider->value(), QStringLiteral("%")));
    m_hueValue->setText(formatVal(m_hueSlider->value(), QStringLiteral("°")));
    m_blurValue->setText(formatVal(m_blurSlider->value(), QStringLiteral("px")));
    m_sharpenValue->setText(formatVal(m_sharpenSlider->value(), QStringLiteral("%")));
    m_loading = false;

    // Executa preview imediata
    onPreviewTimeout();
}

void ImageInspector::retranslateUi()
{
    if (m_quickFiltersSection)
        m_quickFiltersSection->setTitle(m_i18n ? m_i18n->t("editor", "section.quickFilters") : QStringLiteral("Quick 1-Click Filters"));
    if (m_adjustmentsSection)
        m_adjustmentsSection->setTitle(m_i18n ? m_i18n->t("editor", "section.manualAdjustments") : QStringLiteral("Color Adjustments & Filters"));
    if (m_perspectiveSection)
        m_perspectiveSection->setTitle(m_i18n ? m_i18n->t("editor", "section.perspective") : QStringLiteral("3D Tilt & Perspective"));

    if (m_resetTiltBtn)
        m_resetTiltBtn->setText(m_i18n ? m_i18n->t("common", "action.reset") : QStringLiteral("Reset"));

    if (m_brightnessLabel)
        m_brightnessLabel->setText(m_i18n ? m_i18n->t("editor", "image.brightness") : QStringLiteral("Brightness:"));
    if (m_exposureLabel)
        m_exposureLabel->setText(m_i18n ? m_i18n->t("editor", "image.exposure") : QStringLiteral("Exposure:"));
    if (m_shadowsLabel) m_shadowsLabel->setText(m_i18n ? m_i18n->t("editor", "image.shadows") : QStringLiteral("Shadows:"));
    if (m_highlightsLabel) m_highlightsLabel->setText(m_i18n ? m_i18n->t("editor", "image.highlights") : QStringLiteral("Highlights:"));
    if (m_vignetteLabel) m_vignetteLabel->setText(m_i18n ? m_i18n->t("editor", "image.vignette") : QStringLiteral("Vignette:"));
    if (m_contrastLabel)
        m_contrastLabel->setText(m_i18n ? m_i18n->t("editor", "image.contrast") : QStringLiteral("Contrast:"));
    if (m_saturationLabel)
        m_saturationLabel->setText(m_i18n ? m_i18n->t("editor", "image.saturation") : QStringLiteral("Saturation:"));
    if (m_temperatureLabel)
        m_temperatureLabel->setText(m_i18n ? m_i18n->t("editor", "image.temperature") : QStringLiteral("Temperature:"));
    if (m_hueLabel)
        m_hueLabel->setText(m_i18n ? m_i18n->t("editor", "image.hue") : QStringLiteral("Hue / Tint:"));
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
    if (m_presetGrayscaleBtn)
        m_presetGrayscaleBtn->setText(m_i18n ? m_i18n->t("editor", "image.filter.grayscale") : QStringLiteral("P&B"));
    if (m_presetSepiaBtn)
        m_presetSepiaBtn->setText(m_i18n ? m_i18n->t("editor", "image.filter.sepia") : QStringLiteral("Sépia"));
    if (m_presetVintageBtn)
        m_presetVintageBtn->setText(m_i18n ? m_i18n->t("editor", "image.filter.vintage") : QStringLiteral("Vintage"));
    if (m_presetHighContrastBtn)
        m_presetHighContrastBtn->setText(m_i18n ? m_i18n->t("editor", "image.filter.highContrast") : QStringLiteral("Contraste+"));
    if (m_presetVividPopBtn)
        m_presetVividPopBtn->setText(m_i18n ? m_i18n->t("editor", "image.filter.vividPop") : QStringLiteral("Vivid Pop"));
    if (m_presetCyberpunkBtn)
        m_presetCyberpunkBtn->setText(m_i18n ? m_i18n->t("editor", "image.filter.cyberpunk") : QStringLiteral("Cyberpunk"));
    if (m_presetGoldenHourBtn)
        m_presetGoldenHourBtn->setText(m_i18n ? m_i18n->t("editor", "image.filter.goldenHour") : QStringLiteral("Golden Hour"));
    if (m_presetNoirBtn)
        m_presetNoirBtn->setText(m_i18n ? m_i18n->t("editor", "image.filter.noir") : QStringLiteral("Noir"));
}

} // namespace cc
