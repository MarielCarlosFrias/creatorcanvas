#pragma once

#include <QWidget>
#include <QPointer>
#include "core/Document.h"

#include <QTimer>

class QDoubleSpinBox;
class QSlider;
class QLabel;
class QPushButton;
class QGroupBox;

namespace cc {

class I18nService;
class CommandStack;

/// Painel de propriedades para ImageLayers:
/// - Perspectiva 3D e inclinação (shearX, shearY)
/// - Ajustes rápidos de cor (brilho, contraste, saturação, temperatura)
/// - Efeitos de nitidez e desfoque
/// - Filtros de 1 clique (P&B, Sépia, Vintage, Alto Contraste)
class ImageInspector final : public QWidget
{
    Q_OBJECT
public:
    explicit ImageInspector(I18nService* i18n, Document* document,
                           CommandStack* history = nullptr,
                           QWidget* parent = nullptr);

    void setSelectedLayer(const LayerId& id);
    void setDocument(Document* document);
    void refresh();

private:
    void buildUi();
    void retranslateUi();
    bool editingLayer(ImageLayer** out) const;
    void applyAdjustments();
    void applyPreset(int presetIndex);
    void resetAdjustments();
    void schedulePreview();
    void onPreviewTimeout();

    I18nService* m_i18n = nullptr;
    QPointer<Document> m_document;
    CommandStack* m_history = nullptr;
    LayerId m_id;
    LayerId m_originalAssetId;
    LayerId m_currentPreviewAssetId;
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

    QLabel* m_contrastLabel = nullptr;
    QSlider* m_contrastSlider = nullptr;
    QLabel* m_contrastValue = nullptr;

    QLabel* m_saturationLabel = nullptr;
    QSlider* m_saturationSlider = nullptr;
    QLabel* m_saturationValue = nullptr;

    QLabel* m_temperatureLabel = nullptr;
    QSlider* m_temperatureSlider = nullptr;
    QLabel* m_temperatureValue = nullptr;

    QLabel* m_blurLabel = nullptr;
    QSlider* m_blurSlider = nullptr;
    QLabel* m_blurValue = nullptr;

    QLabel* m_sharpenLabel = nullptr;
    QSlider* m_sharpenSlider = nullptr;
    QLabel* m_sharpenValue = nullptr;

    QLabel* m_presetLabel = nullptr;
    QPushButton* m_presetGrayscaleBtn = nullptr;
    QPushButton* m_presetSepiaBtn = nullptr;
    QPushButton* m_presetVintageBtn = nullptr;
    QPushButton* m_presetHighContrastBtn = nullptr;

    QPushButton* m_applyAdjustBtn = nullptr;
    QPushButton* m_resetAdjustBtn = nullptr;

    // Seções recolhíveis
    class CollapsibleSection* m_quickFiltersSection = nullptr;
    class CollapsibleSection* m_adjustmentsSection = nullptr;
    class CollapsibleSection* m_perspectiveSection = nullptr;
};

} // namespace cc
