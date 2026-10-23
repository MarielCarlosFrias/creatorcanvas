#pragma once

#include <QWidget>
#include "tools/CanvasTool.h"

class QLabel;
class QSlider;
class QComboBox;
class QCheckBox;
class QPushButton;
class QStackedWidget;
class QScrollArea;

namespace cc {

class CanvasView;
class I18nService;

class ToolOptionsBar final : public QWidget
{
    Q_OBJECT
public:
    explicit ToolOptionsBar(QWidget* parent = nullptr);
    ~ToolOptionsBar() override = default;

    void setCanvas(CanvasView* canvas);
    void setI18n(I18nService* i18n);

    void setCurrentTool(CanvasTool tool);
    int currentToolIndex() const;

    void applyCompact(bool compact);
    void retranslateUi();

    void setPaintColor(const QColor& color);
    QColor currentPaintColor() const { return m_currentPaintColor; }

private:
    void buildUi();
    void syncInitialValues();

    CanvasView* m_canvas = nullptr;
    I18nService* m_i18n = nullptr;

    QScrollArea* m_scrollArea = nullptr;
    QStackedWidget* m_stack = nullptr;

    QLabel* m_selectHintLabel = nullptr;

    QLabel* m_cropAspectLabel = nullptr;
    QComboBox* m_cropAspectCombo = nullptr;
    QPushButton* m_cropApplyBtn = nullptr;
    QPushButton* m_cropCancelBtn = nullptr;

    QLabel* m_scissorsModeLabel = nullptr;
    QComboBox* m_scissorsModeCombo = nullptr;
    QCheckBox* m_scissorsAutoCropCheck = nullptr;
    QPushButton* m_scissorsApplyBtn = nullptr;
    QPushButton* m_scissorsCancelBtn = nullptr;
    QLabel* m_scissorsHintLabel = nullptr;

    QLabel* m_wandTolLabel = nullptr;
    QSlider* m_wandTolSlider = nullptr;
    QLabel* m_wandTolValueLabel = nullptr;
    QCheckBox* m_wandContiguousCheck = nullptr;
    QLabel* m_wandHintLabel = nullptr;

    QLabel* m_cloneRadiusLabel = nullptr;
    QSlider* m_cloneRadiusSlider = nullptr;
    QLabel* m_cloneRadiusValueLabel = nullptr;
    QLabel* m_cloneHardnessLabel = nullptr;
    QSlider* m_cloneHardnessSlider = nullptr;
    QLabel* m_cloneHardnessValueLabel = nullptr;
    QLabel* m_cloneOpacityLabel = nullptr;
    QSlider* m_cloneOpacitySlider = nullptr;
    QLabel* m_cloneOpacityValueLabel = nullptr;
    QLabel* m_cloneHintLabel = nullptr;

    QLabel* m_paintBrushLabel = nullptr;
    QComboBox* m_paintBrushCombo = nullptr;
    QPushButton* m_paintColorBtn = nullptr;
    QLabel* m_paintSizeLabel = nullptr;
    QSlider* m_paintSizeSlider = nullptr;
    QLabel* m_paintSizeValueLabel = nullptr;
    QLabel* m_paintOpacityLabel = nullptr;
    QSlider* m_paintOpacitySlider = nullptr;
    QLabel* m_paintOpacityValueLabel = nullptr;
    QColor m_currentPaintColor{230, 50, 50};

    QLabel* m_eyedropperHintLabel = nullptr;

    QLabel* m_floodTolLabel = nullptr;
    QSlider* m_floodTolSlider = nullptr;
    QLabel* m_floodTolValueLabel = nullptr;
    QPushButton* m_floodColorBtn = nullptr;

    QLabel* m_marqueeShapeLabel = nullptr;
    QComboBox* m_marqueeShapeCombo = nullptr;
    QLabel* m_marqueeFeatherLabel = nullptr;
    QSlider* m_marqueeFeatherSlider = nullptr;
    QLabel* m_marqueeFeatherValueLabel = nullptr;

    QLabel* m_gradientTypeLabel = nullptr;
    QComboBox* m_gradientTypeCombo = nullptr;
    QPushButton* m_gradientStartColorBtn = nullptr;
    QPushButton* m_gradientEndColorBtn = nullptr;
    QLabel* m_gradientOpacityLabel = nullptr;
    QSlider* m_gradientOpacitySlider = nullptr;
    QLabel* m_gradientOpacityValueLabel = nullptr;
    QColor m_gradientStartColor{61, 126, 255};
    QColor m_gradientEndColor{0, 0, 0, 0};

    QLabel* m_retouchModeLabel = nullptr;
    QComboBox* m_retouchModeCombo = nullptr;
    QLabel* m_retouchSizeLabel = nullptr;
    QSlider* m_retouchSizeSlider = nullptr;
    QLabel* m_retouchSizeValueLabel = nullptr;
    QLabel* m_retouchStrengthLabel = nullptr;
    QSlider* m_retouchStrengthSlider = nullptr;
    QLabel* m_retouchStrengthValueLabel = nullptr;

    QLabel* m_healingModeLabel = nullptr;
    QComboBox* m_healingModeCombo = nullptr;
    QLabel* m_healingRadiusLabel = nullptr;
    QSlider* m_healingRadiusSlider = nullptr;
    QLabel* m_healingRadiusValueLabel = nullptr;
    QLabel* m_healingStrengthLabel = nullptr;
    QSlider* m_healingStrengthSlider = nullptr;
    QLabel* m_healingStrengthValueLabel = nullptr;
};

} // namespace cc
