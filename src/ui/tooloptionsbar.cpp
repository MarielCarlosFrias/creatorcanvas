#include "tooloptionsbar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QScrollArea>
#include <QColorDialog>

#include "canvasview.h"
#include "localization/i18nservice.h"
#include "themeicons.h"

namespace cc {

namespace {

int toolOptionsPageIndex(CanvasTool tool)
{
    switch (tool) {
    case CanvasTool::Select:     return 0;
    case CanvasTool::Crop:       return 1;
    case CanvasTool::Scissors:   return 2;
    case CanvasTool::MagicWand:  return 3;
    case CanvasTool::CloneStamp: return 4;
    case CanvasTool::Paint:      return 5;
    case CanvasTool::FloodFill:  return 6;
    case CanvasTool::Eyedropper: return 7;
    case CanvasTool::Marquee:    return 8;
    case CanvasTool::Gradient:   return 9;
    case CanvasTool::Retouch:    return 10;
    case CanvasTool::Healing:    return 11;
    default:                     return 0;
    }
}

} // namespace

ToolOptionsBar::ToolOptionsBar(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

void ToolOptionsBar::setCanvas(CanvasView* canvas)
{
    m_canvas = canvas;
    syncInitialValues();
}

void ToolOptionsBar::setI18n(I18nService* i18n)
{
    m_i18n = i18n;
    retranslateUi();
}

void ToolOptionsBar::setCurrentTool(CanvasTool tool)
{
    if (m_stack) {
        m_stack->setCurrentIndex(toolOptionsPageIndex(tool));
    }
}

int ToolOptionsBar::currentToolIndex() const
{
    return m_stack ? m_stack->currentIndex() : 0;
}

void ToolOptionsBar::setPaintColor(const QColor& color)
{
    m_currentPaintColor = color;
    if (m_paintColorBtn)
        m_paintColorBtn->setStyleSheet(QStringLiteral("background-color: %1; border: 1px solid #555; border-radius: 3px;").arg(color.name()));
    if (m_floodColorBtn)
        m_floodColorBtn->setStyleSheet(QStringLiteral("background-color: %1; border: 1px solid #555; border-radius: 3px;").arg(color.name()));
    if (m_canvas)
        m_canvas->setPaintColor(color);
}

void ToolOptionsBar::syncInitialValues()
{
    if (!m_canvas)
        return;

    m_canvas->setMarqueeEllipse(false);
    m_canvas->setMarqueeFeather(0);
    m_canvas->setGradientRadial(false);
    m_canvas->setGradientStartColor(m_gradientStartColor);
    m_canvas->setGradientEndColor(m_gradientEndColor);
    m_canvas->setGradientOpacity(1.0);
    m_canvas->setRetouchMode(0);
    m_canvas->setRetouchSize(40);
    m_canvas->setRetouchStrength(0.4);
    m_canvas->setHealingMode(0);
    m_canvas->setHealingRadius(24);
    m_canvas->setHealingStrength(0.85);
}

void ToolOptionsBar::buildUi()
{
    auto* rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    m_stack = new QStackedWidget(this);

    // --- Página 0: Seleção (Select) ---
    QWidget* selectPage = new QWidget(this);
    QHBoxLayout* selectLayout = new QHBoxLayout(selectPage);
    selectLayout->setContentsMargins(8, 2, 8, 2);
    m_selectHintLabel = new QLabel(this);
    m_selectHintLabel->setObjectName(QStringLiteral("HintLabel"));
    selectLayout->addWidget(m_selectHintLabel);
    selectLayout->addStretch();
    m_stack->addWidget(selectPage);

    // --- Página 1: Corte Retangular/Proporção (Crop) ---
    QWidget* cropPage = new QWidget(this);
    QHBoxLayout* cropLayout = new QHBoxLayout(cropPage);
    cropLayout->setContentsMargins(8, 2, 8, 2);
    cropLayout->setSpacing(8);

    m_cropAspectLabel = new QLabel(this);
    m_cropAspectCombo = new QComboBox(this);
    m_cropAspectCombo->addItem(QStringLiteral("Free"), 0.0);
    m_cropAspectCombo->addItem(QStringLiteral("1:1 (Square)"), 1.0);
    m_cropAspectCombo->addItem(QStringLiteral("16:9 (Widescreen)"), 16.0 / 9.0);
    m_cropAspectCombo->addItem(QStringLiteral("4:3 (Standard)"), 4.0 / 3.0);
    m_cropAspectCombo->addItem(QStringLiteral("9:16 (Stories/Reels)"), 9.0 / 16.0);
    m_cropAspectCombo->addItem(QStringLiteral("4:5 (Instagram Portrait)"), 4.0 / 5.0);
    connect(m_cropAspectCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (m_canvas && index >= 0) {
            double ratio = m_cropAspectCombo->itemData(index).toDouble();
            m_canvas->setCropAspectRatio(ratio);
        }
    });

    m_cropApplyBtn = new QPushButton(this);
    m_cropApplyBtn->setIcon(ThemeIcons::actionCheck());
    m_cropApplyBtn->setIconSize(QSize(16, 16));
    m_cropApplyBtn->setStyleSheet(QStringLiteral("background-color: #2b78e4; color: white; font-weight: bold; padding: 4px 12px; border-radius: 4px;"));
    connect(m_cropApplyBtn, &QPushButton::clicked, this, [this] {
        if (m_canvas) m_canvas->applyCrop();
    });

    m_cropCancelBtn = new QPushButton(this);
    m_cropCancelBtn->setIcon(ThemeIcons::actionCancel());
    m_cropCancelBtn->setIconSize(QSize(16, 16));
    connect(m_cropCancelBtn, &QPushButton::clicked, this, [this] {
        if (m_canvas) m_canvas->cancelCrop();
    });

    cropLayout->addWidget(m_cropAspectLabel);
    cropLayout->addWidget(m_cropAspectCombo);
    cropLayout->addWidget(m_cropApplyBtn);
    cropLayout->addWidget(m_cropCancelBtn);
    cropLayout->addStretch();
    m_stack->addWidget(cropPage);

    // --- Página 2: Corte com Tesoura (Scissors Cut) ---
    QWidget* scissorsPage = new QWidget(this);
    QHBoxLayout* scissorsLayout = new QHBoxLayout(scissorsPage);
    scissorsLayout->setContentsMargins(8, 2, 8, 2);
    scissorsLayout->setSpacing(8);

    m_scissorsModeLabel = new QLabel(this);
    m_scissorsModeCombo = new QComboBox(this);
    m_scissorsModeCombo->addItem(QStringLiteral("Keep Inside (Cutout)"), true);
    m_scissorsModeCombo->addItem(QStringLiteral("Erase Inside (Hole)"), false);
    connect(m_scissorsModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (m_canvas && index >= 0) {
            bool keep = m_scissorsModeCombo->itemData(index).toBool();
            m_canvas->setScissorsKeepInside(keep);
        }
    });

    m_scissorsAutoCropCheck = new QCheckBox(this);
    m_scissorsAutoCropCheck->setChecked(true);
    connect(m_scissorsAutoCropCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_canvas) m_canvas->setScissorsAutoCrop(checked);
    });

    m_scissorsApplyBtn = new QPushButton(this);
    m_scissorsApplyBtn->setIcon(ThemeIcons::actionCheck(QColor(0, 0, 0)));
    m_scissorsApplyBtn->setIconSize(QSize(16, 16));
    m_scissorsApplyBtn->setStyleSheet(QStringLiteral("background-color: #00bcd4; color: black; font-weight: bold; padding: 4px 12px; border-radius: 4px;"));
    connect(m_scissorsApplyBtn, &QPushButton::clicked, this, [this] {
        if (m_canvas) m_canvas->applyScissorsCut();
    });

    m_scissorsCancelBtn = new QPushButton(this);
    m_scissorsCancelBtn->setIcon(ThemeIcons::actionCancel());
    m_scissorsCancelBtn->setIconSize(QSize(16, 16));
    connect(m_scissorsCancelBtn, &QPushButton::clicked, this, [this] {
        if (m_canvas) m_canvas->cancelScissorsCut();
    });

    m_scissorsHintLabel = new QLabel(this);
    m_scissorsHintLabel->setObjectName(QStringLiteral("HintLabel"));

    scissorsLayout->addWidget(m_scissorsModeLabel);
    scissorsLayout->addWidget(m_scissorsModeCombo);
    scissorsLayout->addWidget(m_scissorsAutoCropCheck);
    scissorsLayout->addWidget(m_scissorsApplyBtn);
    scissorsLayout->addWidget(m_scissorsCancelBtn);
    scissorsLayout->addWidget(m_scissorsHintLabel);
    scissorsLayout->addStretch();
    m_stack->addWidget(scissorsPage);

    // --- Página 3: Varinha Mágica (Magic Wand) ---
    QWidget* wandPage = new QWidget(this);
    QHBoxLayout* wandLayout = new QHBoxLayout(wandPage);
    wandLayout->setContentsMargins(8, 2, 8, 2);
    wandLayout->setSpacing(8);

    m_wandTolLabel = new QLabel(this);
    m_wandTolSlider = new QSlider(Qt::Horizontal, this);
    m_wandTolSlider->setRange(0, 100);
    m_wandTolSlider->setValue(25);
    m_wandTolSlider->setFixedWidth(120);
    m_wandTolValueLabel = new QLabel(QStringLiteral("25%"), this);
    m_wandTolValueLabel->setFixedWidth(36);

    connect(m_wandTolSlider, &QSlider::valueChanged, this, [this](int val) {
        m_wandTolValueLabel->setText(QString::number(val) + QStringLiteral("%"));
        if (m_canvas) m_canvas->setWandTolerance(val);
    });

    m_wandContiguousCheck = new QCheckBox(this);
    m_wandContiguousCheck->setChecked(true);
    connect(m_wandContiguousCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_canvas) m_canvas->setWandContiguous(checked);
    });

    m_wandHintLabel = new QLabel(this);
    m_wandHintLabel->setObjectName(QStringLiteral("HintLabel"));

    wandLayout->addWidget(m_wandTolLabel);
    wandLayout->addWidget(m_wandTolSlider);
    wandLayout->addWidget(m_wandTolValueLabel);
    wandLayout->addWidget(m_wandContiguousCheck);
    wandLayout->addWidget(m_wandHintLabel);
    wandLayout->addStretch();
    m_stack->addWidget(wandPage);

    // --- Página 4: Carimbo de Clonagem (Clone Stamp) ---
    QWidget* clonePage = new QWidget(this);
    QHBoxLayout* cloneLayout = new QHBoxLayout(clonePage);
    cloneLayout->setContentsMargins(8, 2, 8, 2);
    cloneLayout->setSpacing(8);

    m_cloneRadiusLabel = new QLabel(this);
    m_cloneRadiusSlider = new QSlider(Qt::Horizontal, this);
    m_cloneRadiusSlider->setRange(1, 100);
    m_cloneRadiusSlider->setValue(20);
    m_cloneRadiusSlider->setFixedWidth(78);
    m_cloneRadiusValueLabel = new QLabel(QStringLiteral("20px"), this);
    m_cloneRadiusValueLabel->setFixedWidth(36);

    connect(m_cloneRadiusSlider, &QSlider::valueChanged, this, [this](int val) {
        m_cloneRadiusValueLabel->setText(QString::number(val) + QStringLiteral("px"));
        if (m_canvas) m_canvas->setCloneRadius(val);
    });

    m_cloneHardnessLabel = new QLabel(this);
    m_cloneHardnessSlider = new QSlider(Qt::Horizontal, this);
    m_cloneHardnessSlider->setRange(0, 100);
    m_cloneHardnessSlider->setValue(80);
    m_cloneHardnessSlider->setFixedWidth(78);
    m_cloneHardnessValueLabel = new QLabel(QStringLiteral("80%"), this);
    m_cloneHardnessValueLabel->setFixedWidth(36);

    connect(m_cloneHardnessSlider, &QSlider::valueChanged, this, [this](int val) {
        m_cloneHardnessValueLabel->setText(QString::number(val) + QStringLiteral("%"));
        if (m_canvas) m_canvas->setCloneHardness(val / 100.0);
    });

    m_cloneOpacityLabel = new QLabel(this);
    m_cloneOpacitySlider = new QSlider(Qt::Horizontal, this);
    m_cloneOpacitySlider->setRange(10, 100);
    m_cloneOpacitySlider->setValue(100);
    m_cloneOpacitySlider->setFixedWidth(78);
    m_cloneOpacityValueLabel = new QLabel(QStringLiteral("100%"), this);
    m_cloneOpacityValueLabel->setFixedWidth(40);

    connect(m_cloneOpacitySlider, &QSlider::valueChanged, this, [this](int val) {
        m_cloneOpacityValueLabel->setText(QString::number(val) + QStringLiteral("%"));
        if (m_canvas) m_canvas->setCloneOpacity(val / 100.0);
    });

    m_cloneHintLabel = new QLabel(this);
    m_cloneHintLabel->setObjectName(QStringLiteral("HintLabel"));
    m_cloneRadiusLabel->setFixedWidth(48);
    m_cloneHardnessLabel->setFixedWidth(58);
    m_cloneOpacityLabel->setFixedWidth(52);

    cloneLayout->addWidget(m_cloneRadiusLabel);
    cloneLayout->addWidget(m_cloneRadiusSlider);
    cloneLayout->addWidget(m_cloneRadiusValueLabel);
    cloneLayout->addWidget(m_cloneHardnessLabel);
    cloneLayout->addWidget(m_cloneHardnessSlider);
    cloneLayout->addWidget(m_cloneHardnessValueLabel);
    cloneLayout->addWidget(m_cloneOpacityLabel);
    cloneLayout->addWidget(m_cloneOpacitySlider);
    cloneLayout->addWidget(m_cloneOpacityValueLabel);
    cloneLayout->addWidget(m_cloneHintLabel);
    cloneLayout->addStretch();
    m_stack->addWidget(clonePage);

    // --- Página 5: Pintura (Paint Tool) ---
    QWidget* paintPage = new QWidget(this);
    QHBoxLayout* paintLayout = new QHBoxLayout(paintPage);
    paintLayout->setContentsMargins(8, 2, 8, 2);
    paintLayout->setSpacing(8);

    m_paintBrushLabel = new QLabel(this);
    m_paintBrushCombo = new QComboBox(this);
    m_paintBrushCombo->addItem(QStringLiteral("Brush"), 0);
    m_paintBrushCombo->addItem(QStringLiteral("Pencil"), 1);
    m_paintBrushCombo->addItem(QStringLiteral("Highlighter"), 2);
    m_paintBrushCombo->addItem(QStringLiteral("Airbrush"), 3);
    m_paintBrushCombo->addItem(QStringLiteral("Eraser"), 4);
    connect(m_paintBrushCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (m_canvas && idx >= 0) m_canvas->setPaintBrush(idx);
    });

    m_paintColorBtn = new QPushButton(this);
    m_paintColorBtn->setFixedSize(28, 24);
    m_paintColorBtn->setStyleSheet(QStringLiteral("background-color: %1; border: 1px solid #555; border-radius: 3px;").arg(m_currentPaintColor.name()));
    connect(m_paintColorBtn, &QPushButton::clicked, this, [this] {
        const QColor chosen = QColorDialog::getColor(m_currentPaintColor, this);
        if (chosen.isValid()) {
            m_currentPaintColor = chosen;
            m_paintColorBtn->setStyleSheet(QStringLiteral("background-color: %1; border: 1px solid #555; border-radius: 3px;").arg(chosen.name()));
            if (m_floodColorBtn)
                m_floodColorBtn->setStyleSheet(QStringLiteral("background-color: %1; border: 1px solid #555; border-radius: 3px;").arg(chosen.name()));
            if (m_canvas) m_canvas->setPaintColor(chosen);
        }
    });

    m_paintSizeLabel = new QLabel(this);
    m_paintSizeSlider = new QSlider(Qt::Horizontal, this);
    m_paintSizeSlider->setRange(1, 200);
    m_paintSizeSlider->setValue(20);
    m_paintSizeSlider->setFixedWidth(78);
    m_paintSizeValueLabel = new QLabel(QStringLiteral("20px"), this);
    m_paintSizeValueLabel->setFixedWidth(36);
    connect(m_paintSizeSlider, &QSlider::valueChanged, this, [this](int val) {
        m_paintSizeValueLabel->setText(QString::number(val) + QStringLiteral("px"));
        if (m_canvas) m_canvas->setPaintSize(val);
    });

    m_paintOpacityLabel = new QLabel(this);
    m_paintOpacitySlider = new QSlider(Qt::Horizontal, this);
    m_paintOpacitySlider->setRange(10, 100);
    m_paintOpacitySlider->setValue(100);
    m_paintOpacitySlider->setFixedWidth(78);
    m_paintOpacityValueLabel = new QLabel(QStringLiteral("100%"), this);
    m_paintOpacityValueLabel->setFixedWidth(40);
    connect(m_paintOpacitySlider, &QSlider::valueChanged, this, [this](int val) {
        m_paintOpacityValueLabel->setText(QString::number(val) + QStringLiteral("%"));
        if (m_canvas) m_canvas->setPaintOpacity(val / 100.0);
    });

    paintLayout->addWidget(m_paintBrushLabel);
    paintLayout->addWidget(m_paintBrushCombo);
    paintLayout->addWidget(m_paintColorBtn);
    paintLayout->addWidget(m_paintSizeLabel);
    paintLayout->addWidget(m_paintSizeSlider);
    paintLayout->addWidget(m_paintSizeValueLabel);
    paintLayout->addWidget(m_paintOpacityLabel);
    paintLayout->addWidget(m_paintOpacitySlider);
    paintLayout->addWidget(m_paintOpacityValueLabel);
    paintLayout->addStretch();
    m_stack->addWidget(paintPage);

    // --- Página 6: Preenchimento (Flood Fill Tool) ---
    QWidget* floodPage = new QWidget(this);
    QHBoxLayout* floodLayout = new QHBoxLayout(floodPage);
    floodLayout->setContentsMargins(8, 2, 8, 2);
    floodLayout->setSpacing(8);

    m_floodColorBtn = new QPushButton(this);
    m_floodColorBtn->setFixedSize(28, 24);
    m_floodColorBtn->setStyleSheet(QStringLiteral("background-color: %1; border: 1px solid #555; border-radius: 3px;").arg(m_currentPaintColor.name()));
    connect(m_floodColorBtn, &QPushButton::clicked, this, [this] {
        const QColor chosen = QColorDialog::getColor(m_currentPaintColor, this);
        if (chosen.isValid()) {
            m_currentPaintColor = chosen;
            m_floodColorBtn->setStyleSheet(QStringLiteral("background-color: %1; border: 1px solid #555; border-radius: 3px;").arg(chosen.name()));
            if (m_paintColorBtn)
                m_paintColorBtn->setStyleSheet(QStringLiteral("background-color: %1; border: 1px solid #555; border-radius: 3px;").arg(chosen.name()));
            if (m_canvas) m_canvas->setPaintColor(chosen);
        }
    });

    m_floodTolLabel = new QLabel(this);
    m_floodTolSlider = new QSlider(Qt::Horizontal, this);
    m_floodTolSlider->setRange(0, 100);
    m_floodTolSlider->setValue(25);
    m_floodTolSlider->setFixedWidth(120);
    m_floodTolValueLabel = new QLabel(QStringLiteral("25%"), this);
    m_floodTolValueLabel->setFixedWidth(36);
    connect(m_floodTolSlider, &QSlider::valueChanged, this, [this](int val) {
        m_floodTolValueLabel->setText(QString::number(val) + QStringLiteral("%"));
        if (m_canvas) m_canvas->setWandTolerance(val);
    });

    floodLayout->addWidget(m_floodColorBtn);
    floodLayout->addWidget(m_floodTolLabel);
    floodLayout->addWidget(m_floodTolSlider);
    floodLayout->addWidget(m_floodTolValueLabel);
    floodLayout->addStretch();
    m_stack->addWidget(floodPage);

    // --- Página 7: Conta-gotas (Eyedropper Tool) ---
    QWidget* eyedropperPage = new QWidget(this);
    QHBoxLayout* eyedropperLayout = new QHBoxLayout(eyedropperPage);
    eyedropperLayout->setContentsMargins(8, 2, 8, 2);
    eyedropperLayout->setSpacing(8);
    m_eyedropperHintLabel = new QLabel(QStringLiteral("Click the canvas to sample a color."), this);
    eyedropperLayout->addWidget(m_eyedropperHintLabel);
    eyedropperLayout->addStretch();
    m_stack->addWidget(eyedropperPage);

    // --- Página 8: Seleção retangular/elíptica (Marquee) ---
    QWidget* marqueePage = new QWidget(this);
    auto* marqueeLayout = new QHBoxLayout(marqueePage);
    marqueeLayout->setContentsMargins(8, 2, 8, 2);
    marqueeLayout->setSpacing(8);

    m_marqueeShapeLabel = new QLabel(QStringLiteral("Shape"), this);
    m_marqueeShapeCombo = new QComboBox(this);
    m_marqueeShapeCombo->addItem(QStringLiteral("Rectangle"), 0);
    m_marqueeShapeCombo->addItem(QStringLiteral("Ellipse"), 1);
    connect(m_marqueeShapeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                if (m_canvas) m_canvas->setMarqueeEllipse(index == 1);
            });

    m_marqueeFeatherLabel = new QLabel(QStringLiteral("Feather"), this);
    m_marqueeFeatherSlider = new QSlider(Qt::Horizontal, this);
    m_marqueeFeatherSlider->setRange(0, 40);
    m_marqueeFeatherSlider->setValue(0);
    m_marqueeFeatherSlider->setMinimumWidth(90);
    m_marqueeFeatherValueLabel = new QLabel(QStringLiteral("0 px"), this);
    m_marqueeFeatherValueLabel->setMinimumWidth(44);
    connect(m_marqueeFeatherSlider, &QSlider::valueChanged, this, [this](int value) {
        if (m_marqueeFeatherValueLabel)
            m_marqueeFeatherValueLabel->setText(QStringLiteral("%1 px").arg(value));
        if (m_canvas) m_canvas->setMarqueeFeather(value);
    });

    marqueeLayout->addWidget(m_marqueeShapeLabel);
    marqueeLayout->addWidget(m_marqueeShapeCombo);
    marqueeLayout->addWidget(m_marqueeFeatherLabel);
    marqueeLayout->addWidget(m_marqueeFeatherSlider);
    marqueeLayout->addWidget(m_marqueeFeatherValueLabel);
    marqueeLayout->addStretch();
    m_stack->addWidget(marqueePage);

    // --- Página 9: Gradiente (Gradient) ---
    QWidget* gradientPage = new QWidget(this);
    auto* gradientLayout = new QHBoxLayout(gradientPage);
    gradientLayout->setContentsMargins(8, 2, 8, 2);
    gradientLayout->setSpacing(8);

    m_gradientTypeLabel = new QLabel(QStringLiteral("Type"), this);
    m_gradientTypeCombo = new QComboBox(this);
    m_gradientTypeCombo->addItem(QStringLiteral("Linear"), 0);
    m_gradientTypeCombo->addItem(QStringLiteral("Radial"), 1);
    connect(m_gradientTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                if (m_canvas) m_canvas->setGradientRadial(index == 1);
            });

    m_gradientStartColorBtn = new QPushButton(this);
    m_gradientStartColorBtn->setFixedSize(34, 24);
    m_gradientStartColorBtn->setToolTip(QStringLiteral("Start color"));
    m_gradientStartColorBtn->setStyleSheet(
        QStringLiteral("background-color: %1; border: 1px solid #555; border-radius: 3px;")
            .arg(m_gradientStartColor.name()));
    connect(m_gradientStartColorBtn, &QPushButton::clicked, this, [this] {
        const QColor chosen = QColorDialog::getColor(
            m_gradientStartColor, this,
            m_i18n ? m_i18n->t("editor", "tools.gradient.start")
                   : QStringLiteral("Start color"),
            QColorDialog::ShowAlphaChannel);
        if (!chosen.isValid())
            return;
        m_gradientStartColor = chosen;
        m_gradientStartColorBtn->setStyleSheet(
            QStringLiteral("background-color: %1; border: 1px solid #555; border-radius: 3px;")
                .arg(chosen.name()));
        if (m_canvas) m_canvas->setGradientStartColor(chosen);
    });

    m_gradientEndColorBtn = new QPushButton(this);
    m_gradientEndColorBtn->setFixedSize(34, 24);
    m_gradientEndColorBtn->setToolTip(QStringLiteral("End color"));
    {
        QColor end = m_gradientEndColor;
        end.setAlpha(255);
        m_gradientEndColorBtn->setStyleSheet(
            QStringLiteral("background-color: %1; border: 1px dashed #555; border-radius: 3px;")
                .arg(end.name()));
    }
    connect(m_gradientEndColorBtn, &QPushButton::clicked, this, [this] {
        const QColor chosen = QColorDialog::getColor(
            m_gradientEndColor, this,
            m_i18n ? m_i18n->t("editor", "tools.gradient.end")
                   : QStringLiteral("End color"),
            QColorDialog::ShowAlphaChannel);
        if (!chosen.isValid())
            return;
        m_gradientEndColor = chosen;
        m_gradientEndColorBtn->setStyleSheet(
            (chosen.alpha() < 255
                 ? QStringLiteral("background-color: %1; border: 1px dashed #555; border-radius: 3px;")
                 : QStringLiteral("background-color: %1; border: 1px solid #555; border-radius: 3px;"))
                .arg(chosen.name()));
        if (m_canvas) m_canvas->setGradientEndColor(chosen);
    });

    m_gradientOpacityLabel = new QLabel(QStringLiteral("Opacity"), this);
    m_gradientOpacitySlider = new QSlider(Qt::Horizontal, this);
    m_gradientOpacitySlider->setRange(10, 100);
    m_gradientOpacitySlider->setValue(100);
    m_gradientOpacitySlider->setMinimumWidth(90);
    m_gradientOpacityValueLabel = new QLabel(QStringLiteral("100%"), this);
    m_gradientOpacityValueLabel->setMinimumWidth(44);
    connect(m_gradientOpacitySlider, &QSlider::valueChanged, this, [this](int value) {
        if (m_gradientOpacityValueLabel)
            m_gradientOpacityValueLabel->setText(QStringLiteral("%1%").arg(value));
        if (m_canvas) m_canvas->setGradientOpacity(value / 100.0);
    });

    gradientLayout->addWidget(m_gradientTypeLabel);
    gradientLayout->addWidget(m_gradientTypeCombo);
    gradientLayout->addWidget(m_gradientStartColorBtn);
    gradientLayout->addWidget(m_gradientEndColorBtn);
    gradientLayout->addWidget(m_gradientOpacityLabel);
    gradientLayout->addWidget(m_gradientOpacitySlider);
    gradientLayout->addWidget(m_gradientOpacityValueLabel);
    gradientLayout->addStretch();
    m_stack->addWidget(gradientPage);

    // --- Página 10: Retoque (Retouch) ---
    QWidget* retouchPage = new QWidget(this);
    auto* retouchLayout = new QHBoxLayout(retouchPage);
    retouchLayout->setContentsMargins(8, 2, 8, 2);
    retouchLayout->setSpacing(8);

    m_retouchModeLabel = new QLabel(QStringLiteral("Mode"), this);
    m_retouchModeCombo = new QComboBox(this);
    m_retouchModeCombo->addItem(QStringLiteral("Blur"), 0);
    m_retouchModeCombo->addItem(QStringLiteral("Sharpen"), 1);
    m_retouchModeCombo->addItem(QStringLiteral("Smudge"), 2);
    m_retouchModeCombo->addItem(QStringLiteral("Dodge (lighten)"), 3);
    m_retouchModeCombo->addItem(QStringLiteral("Burn (darken)"), 4);
    m_retouchModeCombo->addItem(QStringLiteral("Sponge (desaturate)"), 5);
    connect(m_retouchModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                if (m_canvas) m_canvas->setRetouchMode(index);
            });

    m_retouchSizeLabel = new QLabel(QStringLiteral("Size"), this);
    m_retouchSizeSlider = new QSlider(Qt::Horizontal, this);
    m_retouchSizeSlider->setRange(4, 200);
    m_retouchSizeSlider->setValue(40);
    m_retouchSizeSlider->setMinimumWidth(90);
    m_retouchSizeValueLabel = new QLabel(QStringLiteral("40 px"), this);
    m_retouchSizeValueLabel->setMinimumWidth(52);
    connect(m_retouchSizeSlider, &QSlider::valueChanged, this, [this](int value) {
        if (m_retouchSizeValueLabel)
            m_retouchSizeValueLabel->setText(QStringLiteral("%1 px").arg(value));
        if (m_canvas) m_canvas->setRetouchSize(value);
    });

    m_retouchStrengthLabel = new QLabel(QStringLiteral("Strength"), this);
    m_retouchStrengthSlider = new QSlider(Qt::Horizontal, this);
    m_retouchStrengthSlider->setRange(5, 100);
    m_retouchStrengthSlider->setValue(40);
    m_retouchStrengthSlider->setMinimumWidth(90);
    m_retouchStrengthValueLabel = new QLabel(QStringLiteral("40%"), this);
    m_retouchStrengthValueLabel->setMinimumWidth(44);
    connect(m_retouchStrengthSlider, &QSlider::valueChanged, this, [this](int value) {
        if (m_retouchStrengthValueLabel)
            m_retouchStrengthValueLabel->setText(QStringLiteral("%1%").arg(value));
        if (m_canvas) m_canvas->setRetouchStrength(value / 100.0);
    });

    retouchLayout->addWidget(m_retouchModeLabel);
    retouchLayout->addWidget(m_retouchModeCombo);
    retouchLayout->addWidget(m_retouchSizeLabel);
    retouchLayout->addWidget(m_retouchSizeSlider);
    retouchLayout->addWidget(m_retouchSizeValueLabel);
    retouchLayout->addWidget(m_retouchStrengthLabel);
    retouchLayout->addWidget(m_retouchStrengthSlider);
    retouchLayout->addWidget(m_retouchStrengthValueLabel);
    retouchLayout->addStretch();
    m_stack->addWidget(retouchPage);

    // --- Página 11: Reparo (Healing) ---
    QWidget* healingPage = new QWidget(this);
    auto* healingLayout = new QHBoxLayout(healingPage);
    healingLayout->setContentsMargins(8, 2, 8, 2);
    healingLayout->setSpacing(8);

    m_healingModeLabel = new QLabel(QStringLiteral("Mode"), this);
    m_healingModeCombo = new QComboBox(this);
    m_healingModeCombo->addItem(QStringLiteral("Spot Healing"), 0);
    m_healingModeCombo->addItem(QStringLiteral("Red Eye"), 1);
    connect(m_healingModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                if (m_canvas) m_canvas->setHealingMode(index);
            });

    m_healingRadiusLabel = new QLabel(QStringLiteral("Radius"), this);
    m_healingRadiusSlider = new QSlider(Qt::Horizontal, this);
    m_healingRadiusSlider->setRange(4, 120);
    m_healingRadiusSlider->setValue(24);
    m_healingRadiusSlider->setMinimumWidth(90);
    m_healingRadiusValueLabel = new QLabel(QStringLiteral("24 px"), this);
    m_healingRadiusValueLabel->setMinimumWidth(52);
    connect(m_healingRadiusSlider, &QSlider::valueChanged, this, [this](int value) {
        if (m_healingRadiusValueLabel)
            m_healingRadiusValueLabel->setText(QStringLiteral("%1 px").arg(value));
        if (m_canvas) m_canvas->setHealingRadius(value);
    });

    m_healingStrengthLabel = new QLabel(QStringLiteral("Strength"), this);
    m_healingStrengthSlider = new QSlider(Qt::Horizontal, this);
    m_healingStrengthSlider->setRange(5, 100);
    m_healingStrengthSlider->setValue(85);
    m_healingStrengthSlider->setMinimumWidth(90);
    m_healingStrengthValueLabel = new QLabel(QStringLiteral("85%"), this);
    m_healingStrengthValueLabel->setMinimumWidth(44);
    connect(m_healingStrengthSlider, &QSlider::valueChanged, this, [this](int value) {
        if (m_healingStrengthValueLabel)
            m_healingStrengthValueLabel->setText(QStringLiteral("%1%").arg(value));
        if (m_canvas) m_canvas->setHealingStrength(value / 100.0);
    });

    healingLayout->addWidget(m_healingModeLabel);
    healingLayout->addWidget(m_healingModeCombo);
    healingLayout->addWidget(m_healingRadiusLabel);
    healingLayout->addWidget(m_healingRadiusSlider);
    healingLayout->addWidget(m_healingRadiusValueLabel);
    healingLayout->addWidget(m_healingStrengthLabel);
    healingLayout->addWidget(m_healingStrengthSlider);
    healingLayout->addWidget(m_healingStrengthValueLabel);
    healingLayout->addStretch();
    m_stack->addWidget(healingPage);

    // Scroll Area wrapper
    m_stack->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setObjectName(QStringLiteral("ToolOptionsScrollArea"));
    m_scrollArea->setWidget(m_stack);
    m_scrollArea->setWidgetResizable(false);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_scrollArea->setStyleSheet(QStringLiteral(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollBar:horizontal { height: 8px; margin: 0 8px; }"
    ));

    rootLayout->addWidget(m_scrollArea);
}

void ToolOptionsBar::applyCompact(bool compact)
{
    const int sliderMin = compact ? 50 : 90;
    const bool hideLabels = compact;

    for (QLabel* label : {m_cropAspectLabel, m_scissorsModeLabel, m_wandTolLabel,
                          m_cloneRadiusLabel, m_cloneHardnessLabel, m_cloneOpacityLabel,
                          m_paintSizeLabel, m_paintOpacityLabel, m_floodTolLabel,
                          m_marqueeShapeLabel, m_marqueeFeatherLabel,
                          m_gradientTypeLabel, m_gradientOpacityLabel,
                          m_retouchModeLabel, m_retouchSizeLabel, m_retouchStrengthLabel,
                          m_healingModeLabel, m_healingRadiusLabel,
                          m_healingStrengthLabel}) {
        if (label)
            label->setVisible(!hideLabels);
    }
    for (QSlider* slider : {m_paintSizeSlider, m_paintOpacitySlider, m_floodTolSlider,
                            m_wandTolSlider, m_cloneRadiusSlider, m_cloneHardnessSlider,
                            m_cloneOpacitySlider, m_marqueeFeatherSlider,
                            m_gradientOpacitySlider, m_retouchSizeSlider,
                            m_retouchStrengthSlider, m_healingRadiusSlider,
                            m_healingStrengthSlider}) {
        if (slider)
            slider->setMinimumWidth(sliderMin);
    }
    for (QComboBox* combo : {m_marqueeShapeCombo, m_gradientTypeCombo,
                             m_retouchModeCombo, m_healingModeCombo,
                             m_paintBrushCombo}) {
        if (combo)
            combo->setMinimumWidth(hideLabels ? 56 : 80);
    }
}

void ToolOptionsBar::retranslateUi()
{
    if (!m_i18n)
        return;

    if (m_selectHintLabel)
        m_selectHintLabel->setText(m_i18n->t("editor", "tools.selectHint"));

    if (m_cropAspectLabel)
        m_cropAspectLabel->setText(m_i18n->t("editor", "tools.crop.aspect") + QStringLiteral(":"));
    if (m_cropApplyBtn)
        m_cropApplyBtn->setText(m_i18n->t("editor", "tools.crop.apply") + QStringLiteral(" (Enter)"));
    if (m_cropCancelBtn)
        m_cropCancelBtn->setText(m_i18n->t("editor", "tools.crop.cancel") + QStringLiteral(" (Esc)"));

    if (m_cropAspectCombo && m_cropAspectCombo->count() >= 6) {
        m_cropAspectCombo->setItemText(0, m_i18n->t("editor", "tools.crop.free"));
        m_cropAspectCombo->setItemText(1, m_i18n->t("editor", "tools.crop.square"));
        m_cropAspectCombo->setItemText(2, m_i18n->t("editor", "tools.crop.widescreen"));
        m_cropAspectCombo->setItemText(3, m_i18n->t("editor", "tools.crop.standard"));
        m_cropAspectCombo->setItemText(4, m_i18n->t("editor", "tools.crop.portrait"));
        m_cropAspectCombo->setItemText(5, QStringLiteral("4:5 (Instagram)"));
    }

    if (m_scissorsModeLabel)
        m_scissorsModeLabel->setText(m_i18n->t("editor", "tools.scissors.mode") + QStringLiteral(":"));
    if (m_scissorsModeCombo && m_scissorsModeCombo->count() >= 2) {
        m_scissorsModeCombo->setItemText(0, m_i18n->t("editor", "tools.scissors.keepInside"));
        m_scissorsModeCombo->setItemText(1, m_i18n->t("editor", "tools.scissors.eraseInside"));
    }
    if (m_scissorsAutoCropCheck)
        m_scissorsAutoCropCheck->setText(m_i18n->t("editor", "tools.scissors.autoCrop"));
    if (m_scissorsApplyBtn)
        m_scissorsApplyBtn->setText(m_i18n->t("editor", "tools.scissors.apply") + QStringLiteral(" (Enter)"));
    if (m_scissorsCancelBtn)
        m_scissorsCancelBtn->setText(m_i18n->t("editor", "tools.scissors.cancel") + QStringLiteral(" (Esc)"));
    if (m_scissorsHintLabel)
        m_scissorsHintLabel->setText(m_i18n->t("editor", "tools.scissors.hint"));

    if (m_wandTolLabel)
        m_wandTolLabel->setText(m_i18n->t("editor", "tools.wand.tolerance") + QStringLiteral(":"));
    if (m_wandContiguousCheck)
        m_wandContiguousCheck->setText(m_i18n->t("editor", "tools.wand.contiguous"));
    if (m_wandHintLabel)
        m_wandHintLabel->setText(m_i18n->t("editor", "tools.wand.hint"));

    if (m_cloneRadiusLabel)
        m_cloneRadiusLabel->setText(m_i18n->t("editor", "tools.clone.radius") + QStringLiteral(":"));
    if (m_cloneHardnessLabel)
        m_cloneHardnessLabel->setText(m_i18n->t("editor", "tools.clone.hardness") + QStringLiteral(":"));
    if (m_cloneOpacityLabel)
        m_cloneOpacityLabel->setText(m_i18n->t("editor", "tools.clone.opacity") + QStringLiteral(":"));
    if (m_cloneHintLabel)
        m_cloneHintLabel->setText(m_i18n->t("editor", "tools.clone.hint"));

    if (m_paintBrushLabel)
        m_paintBrushLabel->setText(m_i18n->t("editor", "tools.paint.brush"));
    if (m_paintSizeLabel)
        m_paintSizeLabel->setText(m_i18n->t("editor", "tools.paint.size"));
    if (m_paintOpacityLabel)
        m_paintOpacityLabel->setText(m_i18n->t("editor", "tools.paint.opacity"));
    if (m_floodTolLabel)
        m_floodTolLabel->setText(m_i18n->t("editor", "tools.flood.tolerance"));

    if (m_marqueeShapeLabel)
        m_marqueeShapeLabel->setText(m_i18n->t("editor", "tools.marquee.shape") + QStringLiteral(":"));
    if (m_marqueeShapeCombo && m_marqueeShapeCombo->count() >= 2) {
        m_marqueeShapeCombo->setItemText(0, m_i18n->t("editor", "tools.marquee.rect"));
        m_marqueeShapeCombo->setItemText(1, m_i18n->t("editor", "tools.marquee.ellipse"));
    }
    if (m_marqueeFeatherLabel)
        m_marqueeFeatherLabel->setText(m_i18n->t("editor", "tools.marquee.feather") + QStringLiteral(":"));

    if (m_gradientTypeLabel)
        m_gradientTypeLabel->setText(m_i18n->t("editor", "tools.gradient.type") + QStringLiteral(":"));
    if (m_gradientTypeCombo && m_gradientTypeCombo->count() >= 2) {
        m_gradientTypeCombo->setItemText(0, m_i18n->t("editor", "tools.gradient.linear"));
        m_gradientTypeCombo->setItemText(1, m_i18n->t("editor", "tools.gradient.radial"));
    }
    if (m_gradientStartColorBtn)
        m_gradientStartColorBtn->setToolTip(m_i18n->t("editor", "tools.gradient.start"));
    if (m_gradientEndColorBtn)
        m_gradientEndColorBtn->setToolTip(m_i18n->t("editor", "tools.gradient.end"));
    if (m_gradientOpacityLabel)
        m_gradientOpacityLabel->setText(m_i18n->t("editor", "tools.gradient.opacity") + QStringLiteral(":"));

    if (m_retouchModeLabel)
        m_retouchModeLabel->setText(m_i18n->t("editor", "tools.retouch.mode") + QStringLiteral(":"));
    if (m_retouchModeCombo && m_retouchModeCombo->count() >= 6) {
        m_retouchModeCombo->setItemText(0, m_i18n->t("editor", "tools.retouch.blur"));
        m_retouchModeCombo->setItemText(1, m_i18n->t("editor", "tools.retouch.sharpen"));
        m_retouchModeCombo->setItemText(2, m_i18n->t("editor", "tools.retouch.smudge"));
        m_retouchModeCombo->setItemText(3, m_i18n->t("editor", "tools.retouch.dodge"));
        m_retouchModeCombo->setItemText(4, m_i18n->t("editor", "tools.retouch.burn"));
        m_retouchModeCombo->setItemText(5, m_i18n->t("editor", "tools.retouch.sponge"));
    }
    if (m_retouchSizeLabel)
        m_retouchSizeLabel->setText(m_i18n->t("editor", "tools.retouch.size") + QStringLiteral(":"));
    if (m_retouchStrengthLabel)
        m_retouchStrengthLabel->setText(m_i18n->t("editor", "tools.retouch.strength") + QStringLiteral(":"));

    if (m_healingModeLabel)
        m_healingModeLabel->setText(m_i18n->t("editor", "tools.healing.mode") + QStringLiteral(":"));
    if (m_healingModeCombo && m_healingModeCombo->count() >= 2) {
        m_healingModeCombo->setItemText(0, m_i18n->t("editor", "tools.healing.spot"));
        m_healingModeCombo->setItemText(1, m_i18n->t("editor", "tools.healing.redEye"));
    }
    if (m_healingRadiusLabel)
        m_healingRadiusLabel->setText(m_i18n->t("editor", "tools.healing.radius") + QStringLiteral(":"));
    if (m_healingStrengthLabel)
        m_healingStrengthLabel->setText(m_i18n->t("editor", "tools.healing.strength") + QStringLiteral(":"));

    if (m_eyedropperHintLabel)
        m_eyedropperHintLabel->setText(m_i18n->t("editor", "tools.eyedropper.hint"));
}

} // namespace cc
