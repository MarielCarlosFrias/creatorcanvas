#include "imageinspector.h"
#include "collapsiblesection.h"
#include "themeicons.h"
#include "core/history/CommandStack.h"
#include "core/history/DocumentCommands.h"
#include "core/image/ImageProcessing.h"
#include "core/selection/SelectionOperations.h"
#include "localization/i18nservice.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QPointer>
#include <QApplication>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QSlider>
#include <QVBoxLayout>
#include <QTabWidget>
#include <QTabBar>
#include <QScrollArea>
#include <QStyle>
#include <QMessageBox>
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

void ImageInspector::commitEffectChange(const std::function<void(ImageEffects&)>& mutate)
{
    ImageLayer* layer = nullptr;
    if (!editingLayer(&layer))
        return;
    const ImageEffects oldFx = layer->effects;
    ImageEffects newFx = oldFx;
    mutate(newFx);
    if (m_history) {
        m_history->execute(std::make_unique<SetLayerImageEffectsCommand>(
            *m_document, m_id, oldFx, newFx));
    } else {
        m_document->setLayerImageEffects(m_id, newFx);
    }
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

    if (m_shadowOn)
        m_shadowOn->setChecked(layer->effects.dropShadow.enabled);
    if (m_shadowDistance)
        m_shadowDistance->setValue(layer->effects.dropShadow.distance);
    if (m_shadowAngle)
        m_shadowAngle->setValue(layer->effects.dropShadow.angleDeg);
    if (m_shadowBlur)
        m_shadowBlur->setValue(layer->effects.dropShadow.blur);
    if (m_shadowOpacitySlider) {
        const int op = static_cast<int>(std::round(layer->effects.dropShadow.opacity * 100.0));
        m_shadowOpacitySlider->setValue(op);
        if (m_shadowOpacityValue)
            m_shadowOpacityValue->setText(QStringLiteral("%1%").arg(op));
    }
    if (m_shadowColor) {
        const QString hex = layer->effects.dropShadow.color.name(QColor::HexRgb);
        m_shadowColor->setText(hex);
        m_shadowColor->setStyleSheet(QStringLiteral("background: %1; color: %2; font-weight: bold; border-radius: 4px; padding: 2px 6px;")
            .arg(hex, layer->effects.dropShadow.color.lightness() > 128 ? QStringLiteral("#000000") : QStringLiteral("#ffffff")));
    }

    if (m_innerShadowOn)
        m_innerShadowOn->setChecked(layer->effects.innerShadow.enabled);
    if (m_innerShadowDistance)
        m_innerShadowDistance->setValue(layer->effects.innerShadow.distance);
    if (m_innerShadowAngle)
        m_innerShadowAngle->setValue(layer->effects.innerShadow.angleDeg);
    if (m_innerShadowBlur)
        m_innerShadowBlur->setValue(layer->effects.innerShadow.blur);
    if (m_innerShadowOpacitySlider) {
        const int op = static_cast<int>(std::round(layer->effects.innerShadow.opacity * 100.0));
        m_innerShadowOpacitySlider->setValue(op);
        if (m_innerShadowOpacityValue)
            m_innerShadowOpacityValue->setText(QStringLiteral("%1%").arg(op));
    }
    if (m_innerShadowColor) {
        const QString hex = layer->effects.innerShadow.color.name(QColor::HexRgb);
        m_innerShadowColor->setText(hex);
        m_innerShadowColor->setStyleSheet(QStringLiteral("background: %1; color: %2; font-weight: bold; border-radius: 4px; padding: 2px 6px;")
            .arg(hex, layer->effects.innerShadow.color.lightness() > 128 ? QStringLiteral("#000000") : QStringLiteral("#ffffff")));
    }

    if (m_colorOverlayOn)
        m_colorOverlayOn->setChecked(layer->effects.colorOverlay.enabled);
    if (m_colorOverlayOpacitySlider) {
        const int op = static_cast<int>(std::round(layer->effects.colorOverlay.opacity * 100.0));
        m_colorOverlayOpacitySlider->setValue(op);
        if (m_colorOverlayOpacityValue)
            m_colorOverlayOpacityValue->setText(QStringLiteral("%1%").arg(op));
    }
    if (m_colorOverlayColor) {
        const QString hex = layer->effects.colorOverlay.color.name(QColor::HexRgb);
        m_colorOverlayColor->setText(hex);
        m_colorOverlayColor->setStyleSheet(QStringLiteral("background: %1; color: %2; font-weight: bold; border-radius: 4px; padding: 2px 6px;")
            .arg(hex, layer->effects.colorOverlay.color.lightness() > 128 ? QStringLiteral("#000000") : QStringLiteral("#ffffff")));
    }

    if (m_blendThisBlackSlider && m_blendThisWhiteSlider) {
        const BlendRange r = layer->blendIf.get(0)[0];
        m_blendThisBlackSlider->setValue(r.black[0]);
        m_blendThisWhiteSlider->setValue(r.white[1]);
        if (m_blendThisBlackValue)
            m_blendThisBlackValue->setText(QString::number(r.black[0]));
        if (m_blendThisWhiteValue)
            m_blendThisWhiteValue->setText(QString::number(r.white[1]));
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
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_subTabs = new QTabWidget(this);
    m_subTabs->tabBar()->setUsesScrollButtons(true);
    m_subTabs->tabBar()->setElideMode(Qt::ElideRight);
    // Estilo via QSS global (QTabBar/QTabWidget::pane) — tema reage em runtime

    // --- ABA 1: Ajustes & Cor (Ajustes de Luz, Cores e Filtros Rápidos) ---
    auto* adjustScroll = new QScrollArea(this);
    adjustScroll->setWidgetResizable(true);
    adjustScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    adjustScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    adjustScroll->setFrameShape(QFrame::NoFrame);
    auto* adjustContainer = new QWidget(adjustScroll);
    auto* adjustLayout = new QVBoxLayout(adjustContainer);
    adjustLayout->setContentsMargins(4, 4, 4, 4);
    adjustLayout->setSpacing(6);

    // --- ABA 2: Estilos & Efeitos (fx) (Contorno, Sombra Projetada, Sombra Interna, Sobreposição) ---
    auto* fxScroll = new QScrollArea(this);
    fxScroll->setWidgetResizable(true);
    fxScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    fxScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    fxScroll->setFrameShape(QFrame::NoFrame);
    auto* fxContainer = new QWidget(fxScroll);
    auto* fxTabContentLayout = new QVBoxLayout(fxContainer);
    fxTabContentLayout->setContentsMargins(4, 4, 4, 4);
    fxTabContentLayout->setSpacing(6);

    // --- ABA 3: Avançado (Chroma Key, 3D Tilt, Mockup, Blend If) ---
    auto* advScroll = new QScrollArea(this);
    advScroll->setWidgetResizable(true);
    advScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    advScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    advScroll->setFrameShape(QFrame::NoFrame);
    auto* advContainer = new QWidget(advScroll);
    auto* advLayout = new QVBoxLayout(advContainer);
    advLayout->setContentsMargins(4, 4, 4, 4);
    advLayout->setSpacing(6);

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

    auto* presetGrid3 = new QHBoxLayout;
    m_presetInvertBtn = new QPushButton(QStringLiteral("Inverter"), this);
    m_presetPosterizeBtn = new QPushButton(QStringLiteral("Posterizar"), this);
    m_presetThresholdBtn = new QPushButton(QStringLiteral("Limiar P&B"), this);
    presetGrid3->addWidget(m_presetInvertBtn);
    presetGrid3->addWidget(m_presetPosterizeBtn);
    presetGrid3->addWidget(m_presetThresholdBtn);
    quickLayout->addLayout(presetGrid3);

    m_quickFiltersSection->setContentLayout(quickLayout);
    adjustLayout->addWidget(m_quickFiltersSection);

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
    fxTabContentLayout->addWidget(m_outlineSection);

    // --- Efeito Sombra Projetada (Drop Shadow) ---
    m_shadowSection = new CollapsibleSection(QStringLiteral("Sombra Projetada (Drop Shadow)"), false, this);
    auto* shadowLayout = new QVBoxLayout;
    shadowLayout->setContentsMargins(4, 4, 4, 4);

    auto* shadowTopRow = new QHBoxLayout;
    m_shadowOn = new QCheckBox(QStringLiteral("Ativar Sombra"), this);
    m_shadowColor = new QPushButton(QStringLiteral("#000000"), this);
    m_shadowColor->setFixedSize(70, 26);
    m_shadowColor->setStyleSheet(QStringLiteral("background: #000000; color: #ffffff; font-weight: bold; border-radius: 4px;"));
    shadowTopRow->addWidget(m_shadowOn);
    shadowTopRow->addStretch();
    shadowTopRow->addWidget(m_shadowColor);
    shadowLayout->addLayout(shadowTopRow);

    auto* shadowForm = new QFormLayout;
    shadowForm->setContentsMargins(0, 0, 0, 0);

    m_shadowDistanceLabel = new QLabel(QStringLiteral("Distância:"), this);
    m_shadowDistance = new QDoubleSpinBox(this);
    m_shadowDistance->setRange(0.0, 100.0);
    m_shadowDistance->setValue(10.0);
    m_shadowDistance->setSuffix(QStringLiteral(" px"));
    shadowForm->addRow(m_shadowDistanceLabel, m_shadowDistance);

    m_shadowAngleLabel = new QLabel(QStringLiteral("Ângulo:"), this);
    m_shadowAngle = new QDoubleSpinBox(this);
    m_shadowAngle->setRange(0.0, 360.0);
    m_shadowAngle->setValue(120.0);
    m_shadowAngle->setSuffix(QStringLiteral("°"));
    shadowForm->addRow(m_shadowAngleLabel, m_shadowAngle);

    m_shadowBlurLabel = new QLabel(QStringLiteral("Desfoque:"), this);
    m_shadowBlur = new QDoubleSpinBox(this);
    m_shadowBlur->setRange(0.0, 60.0);
    m_shadowBlur->setValue(15.0);
    m_shadowBlur->setSuffix(QStringLiteral(" px"));
    shadowForm->addRow(m_shadowBlurLabel, m_shadowBlur);

    m_shadowOpacityLabel = new QLabel(QStringLiteral("Opacidade:"), this);
    auto* shadowOpRow = new QHBoxLayout;
    m_shadowOpacitySlider = new QSlider(Qt::Horizontal, this);
    m_shadowOpacitySlider->setRange(0, 100);
    m_shadowOpacitySlider->setValue(75);
    m_shadowOpacityValue = new QLabel(QStringLiteral("75%"), this);
    m_shadowOpacityValue->setFixedWidth(36);
    shadowOpRow->addWidget(m_shadowOpacitySlider);
    shadowOpRow->addWidget(m_shadowOpacityValue);
    shadowForm->addRow(m_shadowOpacityLabel, shadowOpRow);

    shadowLayout->addLayout(shadowForm);
    m_shadowSection->setContentLayout(shadowLayout);
    fxTabContentLayout->addWidget(m_shadowSection);

    // --- Efeito Sombra Interna (Inner Shadow) ---
    m_innerShadowSection = new CollapsibleSection(QStringLiteral("Sombra Interna (Inner Shadow)"), false, this);
    auto* innerShadowLayout = new QVBoxLayout;
    innerShadowLayout->setContentsMargins(4, 4, 4, 4);

    auto* innerShadowTopRow = new QHBoxLayout;
    m_innerShadowOn = new QCheckBox(QStringLiteral("Ativar Sombra Interna"), this);
    m_innerShadowColor = new QPushButton(QStringLiteral("#000000"), this);
    m_innerShadowColor->setFixedSize(70, 26);
    m_innerShadowColor->setStyleSheet(QStringLiteral("background: #000000; color: #ffffff; font-weight: bold; border-radius: 4px;"));
    innerShadowTopRow->addWidget(m_innerShadowOn);
    innerShadowTopRow->addStretch();
    innerShadowTopRow->addWidget(m_innerShadowColor);
    innerShadowLayout->addLayout(innerShadowTopRow);

    auto* innerShadowForm = new QFormLayout;
    innerShadowForm->setContentsMargins(0, 0, 0, 0);

    m_innerShadowDistanceLabel = new QLabel(QStringLiteral("Distância:"), this);
    m_innerShadowDistance = new QDoubleSpinBox(this);
    m_innerShadowDistance->setRange(0.0, 100.0);
    m_innerShadowDistance->setValue(5.0);
    m_innerShadowDistance->setSuffix(QStringLiteral(" px"));
    innerShadowForm->addRow(m_innerShadowDistanceLabel, m_innerShadowDistance);

    m_innerShadowAngleLabel = new QLabel(QStringLiteral("Ângulo:"), this);
    m_innerShadowAngle = new QDoubleSpinBox(this);
    m_innerShadowAngle->setRange(0.0, 360.0);
    m_innerShadowAngle->setValue(120.0);
    m_innerShadowAngle->setSuffix(QStringLiteral("°"));
    innerShadowForm->addRow(m_innerShadowAngleLabel, m_innerShadowAngle);

    m_innerShadowBlurLabel = new QLabel(QStringLiteral("Desfoque:"), this);
    m_innerShadowBlur = new QDoubleSpinBox(this);
    m_innerShadowBlur->setRange(0.0, 60.0);
    m_innerShadowBlur->setValue(10.0);
    m_innerShadowBlur->setSuffix(QStringLiteral(" px"));
    innerShadowForm->addRow(m_innerShadowBlurLabel, m_innerShadowBlur);

    m_innerShadowOpacityLabel = new QLabel(QStringLiteral("Opacidade:"), this);
    auto* innerShadowOpRow = new QHBoxLayout;
    m_innerShadowOpacitySlider = new QSlider(Qt::Horizontal, this);
    m_innerShadowOpacitySlider->setRange(0, 100);
    m_innerShadowOpacitySlider->setValue(60);
    m_innerShadowOpacityValue = new QLabel(QStringLiteral("60%"), this);
    m_innerShadowOpacityValue->setFixedWidth(36);
    innerShadowOpRow->addWidget(m_innerShadowOpacitySlider);
    innerShadowOpRow->addWidget(m_innerShadowOpacityValue);
    innerShadowForm->addRow(m_innerShadowOpacityLabel, innerShadowOpRow);

    innerShadowLayout->addLayout(innerShadowForm);
    m_innerShadowSection->setContentLayout(innerShadowLayout);
    fxTabContentLayout->addWidget(m_innerShadowSection);

    // --- Efeito Sobreposição de Cor (Color Overlay) ---
    m_colorOverlaySection = new CollapsibleSection(QStringLiteral("Sobreposição de Cor (Color Overlay)"), false, this);
    auto* colorOverlayLayout = new QVBoxLayout;
    colorOverlayLayout->setContentsMargins(4, 4, 4, 4);

    auto* colorOverlayTopRow = new QHBoxLayout;
    m_colorOverlayOn = new QCheckBox(QStringLiteral("Ativar Sobreposição"), this);
    m_colorOverlayColor = new QPushButton(QStringLiteral("#ff0000"), this);
    m_colorOverlayColor->setFixedSize(70, 26);
    m_colorOverlayColor->setStyleSheet(QStringLiteral("background: #ff0000; color: #ffffff; font-weight: bold; border-radius: 4px;"));
    colorOverlayTopRow->addWidget(m_colorOverlayOn);
    colorOverlayTopRow->addStretch();
    colorOverlayTopRow->addWidget(m_colorOverlayColor);
    colorOverlayLayout->addLayout(colorOverlayTopRow);

    auto* colorOverlayForm = new QFormLayout;
    colorOverlayForm->setContentsMargins(0, 0, 0, 0);

    m_colorOverlayOpacityLabel = new QLabel(QStringLiteral("Opacidade:"), this);
    auto* colorOverlayOpRow = new QHBoxLayout;
    m_colorOverlayOpacitySlider = new QSlider(Qt::Horizontal, this);
    m_colorOverlayOpacitySlider->setRange(0, 100);
    m_colorOverlayOpacitySlider->setValue(50);
    m_colorOverlayOpacityValue = new QLabel(QStringLiteral("50%"), this);
    m_colorOverlayOpacityValue->setFixedWidth(36);
    colorOverlayOpRow->addWidget(m_colorOverlayOpacitySlider);
    colorOverlayOpRow->addWidget(m_colorOverlayOpacityValue);
    colorOverlayForm->addRow(m_colorOverlayOpacityLabel, colorOverlayOpRow);

    colorOverlayLayout->addLayout(colorOverlayForm);
    m_colorOverlaySection->setContentLayout(colorOverlayLayout);
    fxTabContentLayout->addWidget(m_colorOverlaySection);

    // --- Chroma Key / Color to Alpha (Recolhido por padrão) ---
    m_chromaKeySection = new CollapsibleSection(QStringLiteral("Chroma Key (Color to Alpha)"), false, this);
    auto* chromaLayout = new QFormLayout;
    chromaLayout->setContentsMargins(4, 4, 4, 4);

    auto* chromaColorRow = new QHBoxLayout;
    m_chromaKeyColorBtn = new QPushButton(this);
    m_chromaKeyColorBtn->setFixedSize(36, 24);
    m_chromaKeyColorBtn->setToolTip(QStringLiteral("Clique para escolher uma cor no diálogo"));
    m_chromaKeyColorBtn->setStyleSheet(QStringLiteral("background: #00ff00; border: 1px solid #555; border-radius: 3px;"));

    m_chromaPipetteBtn = new QPushButton(QStringLiteral(" Pipeta"), this);
    m_chromaPipetteBtn->setIcon(ThemeIcons::toolEyedropper());
    m_chromaPipetteBtn->setToolTip(QStringLiteral("Clique na pipeta e depois clique diretamente no fundo da imagem na tela para capturar o verde exato!"));
    m_chromaPipetteBtn->setObjectName(QStringLiteral("PresetChip"));

    auto* greenBtn = new QPushButton(QStringLiteral("Verde"), this);
    greenBtn->setStyleSheet(QStringLiteral("background: #00ff00; color: black; font-size: 10px; font-weight: bold; border-radius: 3px; padding: 2px 4px;"));
    auto* blueBtn = new QPushButton(QStringLiteral("Azul"), this);
    blueBtn->setStyleSheet(QStringLiteral("background: #0044ff; color: white; font-size: 10px; font-weight: bold; border-radius: 3px; padding: 2px 4px;"));
    auto* whiteBtn = new QPushButton(QStringLiteral("Branco"), this);
    whiteBtn->setStyleSheet(QStringLiteral("background: #ffffff; color: black; font-size: 10px; font-weight: bold; border-radius: 3px; padding: 2px 4px;"));
    auto* blackBtn = new QPushButton(QStringLiteral("Preto"), this);
    blackBtn->setStyleSheet(QStringLiteral("background: #000000; color: white; font-size: 10px; font-weight: bold; border-radius: 3px; padding: 2px 4px;"));

    chromaColorRow->addWidget(m_chromaKeyColorBtn);
    chromaColorRow->addWidget(m_chromaPipetteBtn);
    chromaColorRow->addWidget(greenBtn);
    chromaColorRow->addWidget(blueBtn);
    chromaColorRow->addWidget(whiteBtn);
    chromaColorRow->addWidget(blackBtn);
    chromaColorRow->addStretch();
    chromaLayout->addRow(QStringLiteral("Cor Chave:"), chromaColorRow);

    auto makeChromaSlider = [this](QSlider*& slider, QLabel*& valLabel, int minVal, int maxVal, int defaultVal) {
        auto* row = new QHBoxLayout;
        slider = new QSlider(Qt::Horizontal, this);
        slider->setRange(minVal, maxVal);
        slider->setValue(defaultVal);
        valLabel = new QLabel(QString::number(defaultVal) + QStringLiteral("%"), this);
        valLabel->setFixedWidth(36);
        connect(slider, &QSlider::valueChanged, this, [valLabel](int v) {
            valLabel->setText(QString::number(v) + QStringLiteral("%"));
        });
        row->addWidget(slider);
        row->addWidget(valLabel);
        return row;
    };

    chromaLayout->addRow(QStringLiteral("Tolerância:"), makeChromaSlider(m_chromaToleranceSlider, m_chromaToleranceValue, 0, 100, 20));
    chromaLayout->addRow(QStringLiteral("Transparência:"), makeChromaSlider(m_chromaTranspSlider, m_chromaTranspValue, 0, 100, 0));
    chromaLayout->addRow(QStringLiteral("Opacidade:"), makeChromaSlider(m_chromaOpacitySlider, m_chromaOpacityValue, 0, 100, 100));

    auto* chromaBtnRow = new QHBoxLayout;
    m_chromaApplyBtn = new QPushButton(QStringLiteral("Remover Fundo"), this);
    m_chromaApplyBtn->setObjectName(QStringLiteral("PrimaryButton"));
    m_chromaResetBtn = new QPushButton(QStringLiteral("Restaurar"), this);
    chromaBtnRow->addWidget(m_chromaApplyBtn);
    chromaBtnRow->addWidget(m_chromaResetBtn);
    chromaLayout->addRow(chromaBtnRow);

    m_chromaKeySection->setContentLayout(chromaLayout);
    advLayout->addWidget(m_chromaKeySection);

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
    m_applyAdjustBtn->setObjectName(QStringLiteral("PrimaryButton"));
    m_resetAdjustBtn = new QPushButton(this);
    btnRow->addWidget(m_applyAdjustBtn);
    btnRow->addWidget(m_resetAdjustBtn);
    adjLayout->addRow(btnRow);

    m_adjustmentsSection->setContentLayout(adjLayout);
    adjustLayout->addWidget(m_adjustmentsSection);

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
    advLayout->addWidget(m_perspectiveSection);

    // --- Mockup / Logo (warped, não destrutivo e com undo) ---
    m_mockupSection = new CollapsibleSection(QStringLiteral("Mockup / Substituir Logo"), false, this);
    auto* mockupLayout = new QVBoxLayout;
    mockupLayout->setContentsMargins(4, 4, 4, 4);
    m_mockupHint = new QLabel(QStringLiteral("Importe uma logo e marque uma área livre sobre a imagem. Clique duas vezes ou pressione Enter para confirmar."), this);
    m_mockupHint->setWordWrap(true);
    m_mockupHint->setObjectName(QStringLiteral("HintLabel"));
    mockupLayout->addWidget(m_mockupHint);
    m_mockupButton = new QPushButton(QStringLiteral("Importar logo e criar mockup"), this);
    m_mockupButton->setObjectName(QStringLiteral("PrimaryButton"));
    mockupLayout->addWidget(m_mockupButton);
    m_mockupSection->setContentLayout(mockupLayout);
    advLayout->addWidget(m_mockupSection);

    // --- Blend If: Faixas de Mesclagem de Luminância ---
    m_blendIfSection = new CollapsibleSection(QStringLiteral("Blend If (Mesclagem Avançada)"), false, this);
    auto* blendIfLayout = new QFormLayout;
    blendIfLayout->setContentsMargins(4, 4, 4, 4);

    auto makeBlendSlider = [this](QSlider*& slider, QLabel*& valLabel, int defaultVal) {
        auto* row = new QHBoxLayout;
        slider = new QSlider(Qt::Horizontal, this);
        slider->setRange(0, 255);
        slider->setValue(defaultVal);
        valLabel = new QLabel(QString::number(defaultVal), this);
        valLabel->setFixedWidth(32);
        connect(slider, &QSlider::valueChanged, this, [valLabel](int v) {
            valLabel->setText(QString::number(v));
        });
        row->addWidget(slider);
        row->addWidget(valLabel);
        return row;
    };

    blendIfLayout->addRow(QStringLiteral("Pretos (Esta Camada):"), makeBlendSlider(m_blendThisBlackSlider, m_blendThisBlackValue, 0));
    blendIfLayout->addRow(QStringLiteral("Brancos (Esta Camada):"), makeBlendSlider(m_blendThisWhiteSlider, m_blendThisWhiteValue, 255));

    m_blendIfResetBtn = new QPushButton(QStringLiteral("Restaurar Blend If"), this);
    blendIfLayout->addRow(m_blendIfResetBtn);

    m_blendIfSection->setContentLayout(blendIfLayout);
    advLayout->addWidget(m_blendIfSection);

    // Adiciona stretch ao final de cada aba
    adjustLayout->addStretch();
    fxTabContentLayout->addStretch();
    advLayout->addStretch();

    // Configura os scroll areas com seus containers
    adjustScroll->setWidget(adjustContainer);
    fxScroll->setWidget(fxContainer);
    advScroll->setWidget(advContainer);

    // Adiciona as abas organizadas ao QTabWidget
    m_subTabs->addTab(adjustScroll, QStringLiteral("Ajustes"));
    m_subTabs->addTab(fxScroll, QStringLiteral("Efeitos fx"));
    m_subTabs->addTab(advScroll, QStringLiteral("Avançado"));

    mainLayout->addWidget(m_subTabs);

    // Signal connections
    // A4: antes mutava transform.shearX/Y direto, sem passo de undo.
    connect(m_tiltX, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        ImageLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        const AffineTransform oldT = layer->transform;
        AffineTransform newT = oldT;
        newT.shearX = val;
        if (m_history)
            m_history->execute(std::make_unique<SetLayerTransformCommand>(
                *m_document, m_id, oldT, newT));
        else
            m_document->setLayerTransform(m_id, newT);
    });

    connect(m_tiltY, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        ImageLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        const AffineTransform oldT = layer->transform;
        AffineTransform newT = oldT;
        newT.shearY = val;
        if (m_history)
            m_history->execute(std::make_unique<SetLayerTransformCommand>(
                *m_document, m_id, oldT, newT));
        else
            m_document->setLayerTransform(m_id, newT);
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
    connect(m_presetInvertBtn, &QPushButton::clicked, this, [this] { applyPreset(8); });
    connect(m_presetPosterizeBtn, &QPushButton::clicked, this, [this] { applyPreset(9); });
    connect(m_presetThresholdBtn, &QPushButton::clicked, this, [this] { applyPreset(10); });

    // Conexões de Contorno / Glow (Sticker)
    connect(m_outlineOn, &QCheckBox::toggled, this, [this](bool on) {
        ImageLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        commitEffectChange([on](ImageEffects& fx) { fx.outline.enabled = on; });
    });

    connect(m_outlineColor, &QPushButton::clicked, this, [this] {
        ImageLayer* layer = nullptr;
        if (!editingLayer(&layer)) return;
        const QColor chosen = QColorDialog::getColor(layer->effects.outline.color, this);
        if (!chosen.isValid()) return;
        commitEffectChange([chosen](ImageEffects& fx) { fx.outline.color = chosen; });
        const QString hex = chosen.name(QColor::HexRgb);
        m_outlineColor->setText(hex);
        m_outlineColor->setStyleSheet(QStringLiteral("background: %1; color: %2; font-weight: bold; border-radius: 4px;")
            .arg(hex, chosen.lightness() > 128 ? QStringLiteral("#000000") : QStringLiteral("#ffffff")));
    });

    connect(m_outlineWidth, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        ImageLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        commitEffectChange([val](ImageEffects& fx) { fx.outline.width = val; });
    });

    connect(m_outlineBlur, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        ImageLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        commitEffectChange([val](ImageEffects& fx) { fx.outline.blur = val; });
    });

    // Conexões de Sombra Projetada (Drop Shadow)
    connect(m_shadowOn, &QCheckBox::toggled, this, [this](bool on) {
        ImageLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        commitEffectChange([on](ImageEffects& fx) { fx.dropShadow.enabled = on; });
    });

    connect(m_shadowColor, &QPushButton::clicked, this, [this] {
        ImageLayer* layer = nullptr;
        if (!editingLayer(&layer)) return;
        const QColor chosen = QColorDialog::getColor(layer->effects.dropShadow.color, this);
        if (!chosen.isValid()) return;
        commitEffectChange([chosen](ImageEffects& fx) { fx.dropShadow.color = chosen; });
        const QString hex = chosen.name(QColor::HexRgb);
        m_shadowColor->setText(hex);
        m_shadowColor->setStyleSheet(QStringLiteral("background: %1; color: %2; font-weight: bold; border-radius: 4px;")
            .arg(hex, chosen.lightness() > 128 ? QStringLiteral("#000000") : QStringLiteral("#ffffff")));
    });

    connect(m_shadowDistance, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        ImageLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        commitEffectChange([val](ImageEffects& fx) { fx.dropShadow.distance = val; });
    });

    connect(m_shadowAngle, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        ImageLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        commitEffectChange([val](ImageEffects& fx) { fx.dropShadow.angleDeg = val; });
    });

    connect(m_shadowBlur, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        ImageLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        commitEffectChange([val](ImageEffects& fx) { fx.dropShadow.blur = val; });
    });

    connect(m_shadowOpacitySlider, &QSlider::valueChanged, this, [this](int val) {
        m_shadowOpacityValue->setText(QStringLiteral("%1%").arg(val));
        ImageLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        commitEffectChange([val](ImageEffects& fx) { fx.dropShadow.opacity = val / 100.0; });
    });

    // Conexões de Sombra Interna (Inner Shadow)
    connect(m_innerShadowOn, &QCheckBox::toggled, this, [this](bool on) {
        ImageLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        commitEffectChange([on](ImageEffects& fx) { fx.innerShadow.enabled = on; });
    });

    connect(m_innerShadowColor, &QPushButton::clicked, this, [this] {
        ImageLayer* layer = nullptr;
        if (!editingLayer(&layer)) return;
        const QColor chosen = QColorDialog::getColor(layer->effects.innerShadow.color, this);
        if (!chosen.isValid()) return;
        commitEffectChange([chosen](ImageEffects& fx) { fx.innerShadow.color = chosen; });
        const QString hex = chosen.name(QColor::HexRgb);
        m_innerShadowColor->setText(hex);
        m_innerShadowColor->setStyleSheet(QStringLiteral("background: %1; color: %2; font-weight: bold; border-radius: 4px;")
            .arg(hex, chosen.lightness() > 128 ? QStringLiteral("#000000") : QStringLiteral("#ffffff")));
    });

    connect(m_innerShadowDistance, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        ImageLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        commitEffectChange([val](ImageEffects& fx) { fx.innerShadow.distance = val; });
    });

    connect(m_innerShadowAngle, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        ImageLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        commitEffectChange([val](ImageEffects& fx) { fx.innerShadow.angleDeg = val; });
    });

    connect(m_innerShadowBlur, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        ImageLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        commitEffectChange([val](ImageEffects& fx) { fx.innerShadow.blur = val; });
    });

    connect(m_innerShadowOpacitySlider, &QSlider::valueChanged, this, [this](int val) {
        m_innerShadowOpacityValue->setText(QStringLiteral("%1%").arg(val));
        ImageLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        commitEffectChange([val](ImageEffects& fx) { fx.innerShadow.opacity = val / 100.0; });
    });

    // Conexões de Sobreposição de Cor (Color Overlay)
    connect(m_colorOverlayOn, &QCheckBox::toggled, this, [this](bool on) {
        ImageLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        commitEffectChange([on](ImageEffects& fx) { fx.colorOverlay.enabled = on; });
    });

    connect(m_colorOverlayColor, &QPushButton::clicked, this, [this] {
        ImageLayer* layer = nullptr;
        if (!editingLayer(&layer)) return;
        const QColor chosen = QColorDialog::getColor(layer->effects.colorOverlay.color, this);
        if (!chosen.isValid()) return;
        commitEffectChange([chosen](ImageEffects& fx) { fx.colorOverlay.color = chosen; });
        const QString hex = chosen.name(QColor::HexRgb);
        m_colorOverlayColor->setText(hex);
        m_colorOverlayColor->setStyleSheet(QStringLiteral("background: %1; color: %2; font-weight: bold; border-radius: 4px;")
            .arg(hex, chosen.lightness() > 128 ? QStringLiteral("#000000") : QStringLiteral("#ffffff")));
    });

    connect(m_colorOverlayOpacitySlider, &QSlider::valueChanged, this, [this](int val) {
        m_colorOverlayOpacityValue->setText(QStringLiteral("%1%").arg(val));
        ImageLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        commitEffectChange([val](ImageEffects& fx) { fx.colorOverlay.opacity = val / 100.0; });
    });

    connect(m_mockupButton, &QPushButton::clicked, this, &ImageInspector::openMockupDialog);

    // Conexões de Chroma Key (Color to Alpha)
    auto updateChromaColorBtn = [this](const QColor& c) {
        setChromaKeyColor(c);
    };

    connect(m_chromaKeyColorBtn, &QPushButton::clicked, this, [this, updateChromaColorBtn] {
        const QColor chosen = QColorDialog::getColor(m_chromaKeyColor, this);
        if (chosen.isValid()) updateChromaColorBtn(chosen);
    });
    connect(m_chromaPipetteBtn, &QPushButton::clicked, this, [this] {
        m_pickingChromaColor = true;
        m_chromaPipetteBtn->setProperty("active", true);
        m_chromaPipetteBtn->style()->unpolish(m_chromaPipetteBtn);
        m_chromaPipetteBtn->style()->polish(m_chromaPipetteBtn);
        emit eyedropperRequested();
    });
    connect(greenBtn, &QPushButton::clicked, this, [updateChromaColorBtn] { updateChromaColorBtn(QColor(0, 255, 0)); });
    connect(blueBtn, &QPushButton::clicked, this, [updateChromaColorBtn] { updateChromaColorBtn(QColor(0, 68, 255)); });
    connect(whiteBtn, &QPushButton::clicked, this, [updateChromaColorBtn] { updateChromaColorBtn(QColor(255, 255, 255)); });
    connect(blackBtn, &QPushButton::clicked, this, [updateChromaColorBtn] { updateChromaColorBtn(QColor(0, 0, 0)); });

    connect(m_chromaApplyBtn, &QPushButton::clicked, this, [this] {
        ImageLayer* layer = nullptr;
        if (!editingLayer(&layer) || !m_document || m_originalAssetId.isNull())
            return;

        // Photocraft: locks bloqueiam chroma key também.
        if (!SelectionOperations::canEditPixels(*m_document, m_id)) {
            QMessageBox::warning(this, QStringLiteral("CreatorCanvas"),
                QStringLiteral("The layer is locked (image pixels)."));
            return;
        }

        const QImage current = m_document->assets().decodedImage(layer->assetId);
        if (current.isNull()) return;

        const double tol = m_chromaToleranceSlider->value() / 100.0;
        const double t = m_chromaTranspSlider->value() / 100.0;
        const double o = m_chromaOpacitySlider->value() / 100.0;
        QImage processed = ImageProcessing::colorToAlpha(current, m_chromaKeyColor, t, o, tol);
        if (processed.isNull()) return;
        // Seleção ativa restringe o chroma key à área selecionada.
        if (!m_document->pixelSelection().isNull())
            processed = SelectionOperations::applySelectionMask(
                current, processed, m_document->pixelSelection());

        const LayerId origAssetId = layer->assetId;
        const LayerId newAssetId = m_document->assets().addImage(processed);

        if (m_history) {
            m_history->execute(std::make_unique<ModifyImageLayerCommand>(
                *m_document, m_id,
                origAssetId, layer->naturalWidth, layer->naturalHeight, layer->transform,
                newAssetId, processed.width(), processed.height(), layer->transform,
                QStringLiteral("image.chromaKey")));
        } else {
            m_document->setImageLayerAsset(m_id, newAssetId, processed.width(), processed.height());
            m_document->touchLayer(m_id);
        }
        m_originalAssetId = newAssetId;
    });

    connect(m_chromaResetBtn, &QPushButton::clicked, this, [this] {
        ImageLayer* layer = nullptr;
        if (!editingLayer(&layer) || !m_document || m_originalAssetId.isNull())
            return;
        m_chromaToleranceSlider->setValue(20);
        m_chromaTranspSlider->setValue(0);
        m_chromaOpacitySlider->setValue(100);
    });

    auto applyBlendIfChange = [this] {
        ImageLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer) || !m_document)
            return;
        BlendIf next = layer->blendIf;
        BlendRange thisRange;
        thisRange.black[0] = static_cast<quint8>(m_blendThisBlackSlider->value());
        thisRange.black[1] = static_cast<quint8>(m_blendThisBlackSlider->value());
        thisRange.white[0] = static_cast<quint8>(m_blendThisWhiteSlider->value());
        thisRange.white[1] = static_cast<quint8>(m_blendThisWhiteSlider->value());
        next.set(0, {thisRange, next.get(0)[1]});

        if (m_history) {
            m_history->execute(std::make_unique<LayerBlendIfCommand>(
                *m_document, m_id, QStringLiteral("layer.blendIf"),
                &Document::setLayerBlendIf, layer->blendIf, next));
        } else {
            m_document->setLayerBlendIf(m_id, next);
        }
    };

    connect(m_blendThisBlackSlider, &QSlider::valueChanged, this, applyBlendIfChange);
    connect(m_blendThisWhiteSlider, &QSlider::valueChanged, this, applyBlendIfChange);
    connect(m_blendIfResetBtn, &QPushButton::clicked, this, [this] {
        m_blendThisBlackSlider->setValue(0);
        m_blendThisWhiteSlider->setValue(255);
    });
}

void ImageInspector::setChromaKeyColor(const QColor& color)
{
    if (!color.isValid()) return;
    m_chromaKeyColor = color;
    if (m_chromaKeyColorBtn) {
        m_chromaKeyColorBtn->setStyleSheet(QStringLiteral("background: %1; border: 1px solid #555; border-radius: 3px;").arg(color.name()));
    }
    if (m_chromaPipetteBtn) {
        m_chromaPipetteBtn->setProperty("active", false);
        m_chromaPipetteBtn->style()->unpolish(m_chromaPipetteBtn);
        m_chromaPipetteBtn->style()->polish(m_chromaPipetteBtn);
    }
    m_pickingChromaColor = false;
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
    // B5: prévia transitória — o autosave continua rodando durante o
    // diálogo modal, mas esta camada NUNCA é gravada em save/recuperação
    // enquanto o usuário não confirmar.
    overlay->transient = true;
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
    if (accepted) {
        // Promovida a definitiva: sai do modo transitório antes de entrar
        // na história (e, com ela, no documento persistido).
        finalLayer->transient = false;
        if (m_history)
            m_history->execute(std::make_unique<AddLayerCommand>(*m_document, std::move(finalLayer)));
        else
            m_document->addLayer(std::move(finalLayer));
    }
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
    // UAF fix: antes o thread desanexado capturava `this` cru — se o
    // inspector fosse destruído durante o cálculo (ex.: sair do app logo
    // após arrastar um slider), invokeMethod dereferenciava um QObject
    // liberado. Agora: QPointer + post via qApp com guarda de shutdown.
    QPointer<ImageInspector> self(this);
    std::thread([self, generation, layerId, future = std::move(future)]() mutable {
        QImage adjusted = future.get();
        if (!QApplication::instance())
            return; // app em teardown: não posta em fila de objeto morto
        QMetaObject::invokeMethod(QApplication::instance(), [self, generation, layerId, adjusted = std::move(adjusted)]() mutable {
            if (!self)
                return;
            if (generation != self->m_previewGeneration.load() || layerId != self->m_id || !self->m_document)
                return;
            self->m_latestPreviewImage = std::move(adjusted);
            emit self->imagePreviewReady(layerId, self->m_latestPreviewImage);
        }, Qt::QueuedConnection);
    }).detach();
}

void ImageInspector::applyAdjustments()
{
    m_previewTimer.stop();
    ImageLayer* layer = nullptr;
    if (!editingLayer(&layer) || !m_document || m_originalAssetId.isNull())
        return;

    // Photocraft/Photoshop: locks bloqueiam qualquer edição de pixels —
    // incluindo ajustes do inspector (antes: filtro aplicava mesmo trancado).
    QString whyNot;
    if (!SelectionOperations::canEditPixels(*m_document, m_id, &whyNot)) {
        QMessageBox::warning(this, QStringLiteral("CreatorCanvas"), whyNot);
        // Reseta a prévia: os sliders voltam ao neutro sem aplicar nada.
        m_latestPreviewImage = QImage();
        m_currentPreviewAssetId = LayerId();
        if (!m_originalAssetId.isNull() && m_document) {
            m_document->setImageLayerAsset(m_id, m_originalAssetId,
                layer->naturalWidth, layer->naturalHeight);
        }
        emit imagePreviewCleared();
        return;
    }

    if (m_latestPreviewImage.isNull() && (m_currentPreviewAssetId.isNull() || m_currentPreviewAssetId == m_originalAssetId))
        return;

    if (m_currentPreviewAssetId.isNull() && !m_latestPreviewImage.isNull())
        m_currentPreviewAssetId = m_document->assets().addImage(m_latestPreviewImage);

    QImage finalImg = m_latestPreviewImage.isNull()
        ? m_document->assets().decodedImage(m_currentPreviewAssetId)
        : m_latestPreviewImage;
    if (finalImg.isNull())
        return;

    // Photocraft/Photoshop: a seleção ativa restringe o ajuste à área
    // selecionada (antes: aplicava na imagem INTEIRA mesmo com seleção).
    if (!m_document->pixelSelection().isNull()) {
        const QImage originalImg =
            m_document->assets().decodedImage(m_originalAssetId);
        finalImg = SelectionOperations::applySelectionMask(
            originalImg, finalImg, m_document->pixelSelection());
        // Re-register the masked result as the final asset.
        m_currentPreviewAssetId = m_document->assets().addImage(finalImg);
    }

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
    case 8: // Invert
    case 9: // Posterize
    case 10: { // Threshold
        ImageLayer* layer = nullptr;
        if (editingLayer(&layer) && m_document) {
            // Photocraft: locks + seleção restringem filtros predefinidos.
            if (!SelectionOperations::canEditPixels(*m_document, m_id)) {
                QMessageBox::warning(this, QStringLiteral("CreatorCanvas"),
                    QStringLiteral("The layer is locked (image pixels)."));
                return;
            }
            const QImage base = m_document->assets().decodedImage(layer->assetId);
            if (!base.isNull()) {
                ImageProcessing::PresetFilter filter = ImageProcessing::PresetFilter::Invert;
                if (presetIndex == 9) filter = ImageProcessing::PresetFilter::Posterize;
                else if (presetIndex == 10) filter = ImageProcessing::PresetFilter::Threshold;
                QImage filtered = ImageProcessing::applyPresetFilter(base, filter);
                if (!m_document->pixelSelection().isNull())
                    filtered = SelectionOperations::applySelectionMask(
                        base, filtered, m_document->pixelSelection());
                const LayerId newAsset = m_document->assets().addImage(filtered, QStringLiteral("png"));
                // A4: antes mutava sem comando — "Inverter cores" era
                // impossível de desfazer. O asset antigo fica no store e o
                // undo só restaura id + dimensões.
                if (m_history) {
                    m_history->execute(std::make_unique<SetLayerImageAssetCommand>(
                        *m_document, m_id, layer->assetId,
                        layer->naturalWidth, layer->naturalHeight,
                        newAsset, filtered.width(), filtered.height()));
                } else {
                    m_document->setImageLayerAsset(m_id, newAsset, filtered.width(), filtered.height());
                }
            }
        }
        m_loading = false;
        return;
    }
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
    if (m_chromaKeySection)
        m_chromaKeySection->setTitle(m_i18n ? m_i18n->t("editor", "section.chromaKey") : QStringLiteral("Chroma Key (Color to Alpha)"));

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

    const bool portuguese = m_i18n && m_i18n->currentLanguage() == QStringLiteral("pt-BR");
    if (m_blendIfSection)
        m_blendIfSection->setTitle(portuguese ? QStringLiteral("Blend If (Mesclagem Avançada)") : QStringLiteral("Blend If (Luminance Range)"));
    if (m_blendIfResetBtn)
        m_blendIfResetBtn->setText(portuguese ? QStringLiteral("Restaurar Blend If") : QStringLiteral("Reset Blend If"));
}

} // namespace cc
