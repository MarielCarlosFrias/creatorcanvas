#pragma once

#include <QWidget>
#include <QPointer>
#include <QPolygonF>
#include <QImage>
#include "core/Document.h"
#include "core/DocumentController.h"

#include <QTimer>
#include <atomic>
#include <functional>

class QDoubleSpinBox;
class QSlider;
class QLabel;
class QPushButton;
class QGroupBox;
class QCheckBox;
class QTabWidget;
class QScrollArea;

namespace cc {

class I18nService;
class CommandStack;

/// Painel de propriedades para ImageLayers:
/// - Perspectiva 3D e inclinação (shearX, shearY)
/// - Ajustes rápidos de cor (brilho, contraste, saturação, temperatura)
/// - Efeitos de nitidez e desfoque
/// - Filtros de 1 clique (P&B, Sépia, Vintage, Alto Contraste)
class ImageInspector final : public QWidget, public IDocumentObserver
{
    Q_OBJECT
public:
    explicit ImageInspector(I18nService* i18n, Document* document,
                           CommandStack* history = nullptr,
                           QWidget* parent = nullptr);

    void onDocumentChanged(Document* doc) override { setDocument(doc); }
    void setSelectedLayer(const LayerId& id);
    void setDocument(Document* document);
    void refresh();
    void setMockupArea(const QPolygonF& corners);
    void openMockupDialog();
    void applyPreset(int presetIndex);

public slots:
    void setChromaKeyColor(const QColor& color);

signals:
    void eyedropperRequested();
    void mockupPlacementRequested();
    void imagePreviewReady(const cc::LayerId& id, const QImage& image);
    void imagePreviewCleared();

private:
    void buildUi();
    void retranslateUi();
    bool editingLayer(ImageLayer** out) const;
    /// A4: aplica uma mutação de ImageEffects SEMPRE via comando de undo
    /// (SetLayerImageEffectsCommand, com merge para arrastar sliders).
    void commitEffectChange(const std::function<void(ImageEffects&)>& mutate);
    void applyAdjustments();
    void resetAdjustments();
    void schedulePreview();
    void onPreviewTimeout();
    void openPuppetWarpDialog();

    I18nService* m_i18n = nullptr;
    QPointer<Document> m_document;
    CommandStack* m_history = nullptr;
    LayerId m_id;
    LayerId m_originalAssetId;
    LayerId m_currentPreviewAssetId;
    QImage m_originalPreviewImage;
    QImage m_latestPreviewImage;
    std::atomic_uint m_previewGeneration{0};
    QTimer m_previewTimer;
    bool m_loading = false;

    // Perspectiva 3D / Tilt
    QGroupBox* m_perspectiveGroup = nullptr;
    QLabel* m_tiltXLabel = nullptr;
    QDoubleSpinBox* m_tiltX = nullptr;
    QLabel* m_tiltYLabel = nullptr;
    QDoubleSpinBox* m_tiltY = nullptr;
    QPushButton* m_resetTiltBtn = nullptr;

    // Ajustes Rápidos & Filtros
    QGroupBox* m_adjustGroup = nullptr;
    QLabel* m_brightnessLabel = nullptr;
    QSlider* m_brightnessSlider = nullptr;
    QLabel* m_brightnessValue = nullptr;

    QLabel* m_exposureLabel = nullptr;
    QSlider* m_exposureSlider = nullptr;
    QLabel* m_exposureValue = nullptr;

    QLabel* m_contrastLabel = nullptr;
    QSlider* m_contrastSlider = nullptr;
    QLabel* m_contrastValue = nullptr;

    QLabel* m_saturationLabel = nullptr;
    QSlider* m_saturationSlider = nullptr;
    QLabel* m_saturationValue = nullptr;

    QLabel* m_temperatureLabel = nullptr;
    QSlider* m_temperatureSlider = nullptr;
    QLabel* m_temperatureValue = nullptr;

    QLabel* m_hueLabel = nullptr;
    QSlider* m_hueSlider = nullptr;
    QLabel* m_hueValue = nullptr;

    QLabel* m_blurLabel = nullptr;
    QSlider* m_blurSlider = nullptr;
    QLabel* m_blurValue = nullptr;

    QLabel* m_sharpenLabel = nullptr;
    QSlider* m_sharpenSlider = nullptr;
    QLabel* m_sharpenValue = nullptr;
    QLabel* m_shadowsLabel = nullptr;
    QSlider* m_shadowsSlider = nullptr;
    QLabel* m_shadowsValue = nullptr;
    QLabel* m_highlightsLabel = nullptr;
    QSlider* m_highlightsSlider = nullptr;
    QLabel* m_highlightsValue = nullptr;
    QLabel* m_vignetteLabel = nullptr;
    QSlider* m_vignetteSlider = nullptr;
    QLabel* m_vignetteValue = nullptr;

    QLabel* m_presetLabel = nullptr;
    QPushButton* m_presetGrayscaleBtn = nullptr;
    QPushButton* m_presetSepiaBtn = nullptr;
    QPushButton* m_presetVintageBtn = nullptr;
    QPushButton* m_presetHighContrastBtn = nullptr;
    QPushButton* m_presetVividPopBtn = nullptr;
    QPushButton* m_presetCyberpunkBtn = nullptr;
    QPushButton* m_presetGoldenHourBtn = nullptr;
    QPushButton* m_presetNoirBtn = nullptr;
    QPushButton* m_presetInvertBtn = nullptr;
    QPushButton* m_presetPosterizeBtn = nullptr;
    QPushButton* m_presetThresholdBtn = nullptr;

    QPushButton* m_applyAdjustBtn = nullptr;
    QPushButton* m_resetAdjustBtn = nullptr;

    // Efeito Contorno & Glow (Sticker Effect)
    class CollapsibleSection* m_outlineSection = nullptr;
    QCheckBox* m_outlineOn = nullptr;
    QPushButton* m_outlineColor = nullptr;
    QDoubleSpinBox* m_outlineWidth = nullptr;
    QDoubleSpinBox* m_outlineBlur = nullptr;
    QLabel* m_outlineWidthLabel = nullptr;
    QLabel* m_outlineBlurLabel = nullptr;

    // Efeito Sombra Projetada (Drop Shadow)
    class CollapsibleSection* m_shadowSection = nullptr;
    QCheckBox* m_shadowOn = nullptr;
    QPushButton* m_shadowColor = nullptr;
    QDoubleSpinBox* m_shadowDistance = nullptr;
    QDoubleSpinBox* m_shadowAngle = nullptr;
    QDoubleSpinBox* m_shadowBlur = nullptr;
    QSlider* m_shadowOpacitySlider = nullptr;
    QLabel* m_shadowOpacityValue = nullptr;
    QLabel* m_shadowDistanceLabel = nullptr;
    QLabel* m_shadowAngleLabel = nullptr;
    QLabel* m_shadowBlurLabel = nullptr;
    QLabel* m_shadowOpacityLabel = nullptr;

    // Efeito Sombra Interna (Inner Shadow)
    class CollapsibleSection* m_innerShadowSection = nullptr;
    QCheckBox* m_innerShadowOn = nullptr;
    QPushButton* m_innerShadowColor = nullptr;
    QDoubleSpinBox* m_innerShadowDistance = nullptr;
    QDoubleSpinBox* m_innerShadowAngle = nullptr;
    QDoubleSpinBox* m_innerShadowBlur = nullptr;
    QSlider* m_innerShadowOpacitySlider = nullptr;
    QLabel* m_innerShadowOpacityValue = nullptr;
    QLabel* m_innerShadowDistanceLabel = nullptr;
    QLabel* m_innerShadowAngleLabel = nullptr;
    QLabel* m_innerShadowBlurLabel = nullptr;
    QLabel* m_innerShadowOpacityLabel = nullptr;

    // Efeito Sobreposição de Cor (Color Overlay)
    class CollapsibleSection* m_colorOverlaySection = nullptr;
    QCheckBox* m_colorOverlayOn = nullptr;
    QPushButton* m_colorOverlayColor = nullptr;
    QSlider* m_colorOverlayOpacitySlider = nullptr;
    QLabel* m_colorOverlayOpacityValue = nullptr;
    QLabel* m_colorOverlayOpacityLabel = nullptr;

    // Chroma Key / Color to Alpha
    class CollapsibleSection* m_chromaKeySection = nullptr;
    QPushButton* m_chromaKeyColorBtn = nullptr;
    QPushButton* m_chromaPipetteBtn = nullptr;
    QColor m_chromaKeyColor{0, 255, 0};
    QSlider* m_chromaToleranceSlider = nullptr;
    QLabel* m_chromaToleranceValue = nullptr;
    QSlider* m_chromaTranspSlider = nullptr;
    QLabel* m_chromaTranspValue = nullptr;
    QSlider* m_chromaOpacitySlider = nullptr;
    QLabel* m_chromaOpacityValue = nullptr;
    QPushButton* m_chromaApplyBtn = nullptr;
    QPushButton* m_chromaResetBtn = nullptr;
    bool m_pickingChromaColor = false;

    // Seções recolhíveis
    class CollapsibleSection* m_quickFiltersSection = nullptr;
    class CollapsibleSection* m_adjustmentsSection = nullptr;
    class CollapsibleSection* m_perspectiveSection = nullptr;
    class CollapsibleSection* m_mockupSection = nullptr;
    QPushButton* m_mockupButton = nullptr;
    QPushButton* m_puppetWarpButton = nullptr;
    QLabel* m_mockupHint = nullptr;
    QImage m_pendingMockupLogo;

    // Blend If (Mesclagem Avançada por Luminância)
    class CollapsibleSection* m_blendIfSection = nullptr;
    QSlider* m_blendThisBlackSlider = nullptr;
    QLabel* m_blendThisBlackValue = nullptr;
    QSlider* m_blendThisWhiteSlider = nullptr;
    QLabel* m_blendThisWhiteValue = nullptr;
    QPushButton* m_blendIfResetBtn = nullptr;

    // Sub-abas organizadas estilo PhotoCraft
    QTabWidget* m_subTabs = nullptr;
};

} // namespace cc
