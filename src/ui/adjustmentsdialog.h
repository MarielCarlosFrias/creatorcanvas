#pragma once

#include "core/image/AdjustmentEngine.h"
#include "core/layers/Layer.h"

#include <QDialog>
#include <QImage>

#include <memory>

#include <QResizeEvent>

class QComboBox;
class QCheckBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSlider;
class QStackedWidget;
class QSpinBox;
class QDoubleSpinBox;
class QVBoxLayout;

namespace cc {

class CurveEditor;
class HistogramView;
class I18nService;

/// Resultado do diálogo: tipo de ajuste + parâmetros serializados + o
/// caminho escolhido (camada de ajuste não-destrutiva ou asset cozido).
struct AdjustmentRequest
{
    AdjustmentType type = AdjustmentType::BrightnessContrast;
    AdjustmentParams params;
    bool nonDestructive = true;
    bool valid = false;
};

/// Diálogo de ajustes de imagem (estilo Photoshop). Selecciona o tipo,
/// afina os parâmetros com prévia ao vivo e aplica — por omissão criando
/// uma camada de ajuste ligada à camada de imagem (não-destrutivo), ou
/// opcionalmente cozendo o resultado num novo asset (comportamento antigo
/// dos filtros do inspector).
class AdjustmentsDialog final : public QDialog
{
    Q_OBJECT

public:
    AdjustmentsDialog(const QImage& source, AdjustmentType initial,
                      I18nService* i18n = nullptr, QWidget* parent = nullptr,
                      bool advancedMode = false);
    ~AdjustmentsDialog() override = default;

    AdjustmentRequest request() const { return m_request; }

private slots:
    void onTypeChanged(int row);
    void onChannelChanged(int index);
    void onParamChanged();
    void updatePreview();

private:
    void buildUi();
    QWidget* buildSlidersPage(QSlider*& outA, QSlider*& outB,
                              const QString& labelA, const QString& labelB);
    QWidget* buildSimplePage();
    QWidget* buildLevelsPage();
    QWidget* buildCurvesPage();
    QWidget* buildGradientMapPage();
    QWidget* buildShadowsHighlightsPage();
    QWidget* buildVignettePage();
    QWidget* buildChannelMixerPage();
    void collectParams();
    /// (Re)calcula o histograma da prévia actual (modo avançado).
    void updateHistogram();

public:
    void accept() override;

protected:
    void resizeEvent(QResizeEvent* event) override;
    QString tr(const char* key, const QString& fallback) const;

    // Acesso aos níveis/canaas activos (master + RGB).
    LevelsChannel& activeLevels();
    std::vector<CurvePoint>& activeCurve();
    QString channelPrefix() const;

    QImage m_source;
    QImage m_preview;
    AdjustmentRequest m_request;
    I18nService* m_i18n = nullptr;

    /// O histograma é ferramenta profissional: só existe em modo avançado.
    bool m_advancedMode = false;

    /// Verdadeiro enquanto a UI está a ser construída. Os sliders emitem
    /// valueChanged ao serem inicializados (ex.: inWhite=255), e os handlers
    /// NÃO podem correr nessa altura — os ponteiros dos outros controlos
    /// ainda não existem. Foi isto que fez o diálogo abrir directamente em
    /// "Níveis" crashar.
    bool m_buildingUi = false;

    QListWidget* m_typeList = nullptr;
    QStackedWidget* m_pages = nullptr;
    QLabel* m_previewLabel = nullptr;
    QCheckBox* m_nonDestructiveCheck = nullptr;

    // Página de sliders simples: brilho/contraste e matiz/saturação.
    QSlider* m_bcSliderA = nullptr;
    QSlider* m_bcSliderB = nullptr;
    QSlider* m_hsSliderA = nullptr;
    QSlider* m_hsSliderB = nullptr;

    // Página de níveis/canaas.
    QComboBox* m_channelCombo = nullptr;
    QSlider* m_inBlack = nullptr;
    QSlider* m_inWhite = nullptr;
    QDoubleSpinBox* m_gamma = nullptr;
    QSlider* m_outBlack = nullptr;
    QSlider* m_outWhite = nullptr;
    LevelsChannel m_levelsMaster;
    LevelsChannel m_levelsR;
    LevelsChannel m_levelsG;
    LevelsChannel m_levelsB;

    // Página de curvas.
    CurveEditor* m_curve = nullptr;
    HistogramView* m_histogram = nullptr;
    QLabel* m_histogramLabel = nullptr;
    std::vector<CurvePoint> m_curveMaster;
    std::vector<CurvePoint> m_curveR;
    std::vector<CurvePoint> m_curveG;
    std::vector<CurvePoint> m_curveB;

    // Páginas novas: sombras/realces, vinheta e channel mixer.
    QSlider* m_shadowSlider = nullptr;
    QSlider* m_highlightSlider = nullptr;
    QSlider* m_vignetteSlider = nullptr;
    QSlider* m_cmRed = nullptr;
    QSlider* m_cmGreen = nullptr;
    QSlider* m_cmBlue = nullptr;

    // Página de mapa de gradiente.
    QPushButton* m_color1Button = nullptr;
    QPushButton* m_color2Button = nullptr;
    QCheckBox* m_reverseCheck = nullptr;
    QColor m_color1 = Qt::black;
    QColor m_color2 = Qt::white;
};

} // namespace cc
