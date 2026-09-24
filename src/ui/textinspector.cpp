#include "textinspector.h"
#include "collapsiblesection.h"
#include "localization/i18nservice.h"

#include <QColorDialog>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

namespace cc {

TextInspector::TextInspector(I18nService* i18n, Document* document,
                             QWidget* parent)
    : QWidget(parent)
    , m_i18n(i18n)
    , m_document(document)
{
    buildUi();

    if (m_i18n)
        connect(m_i18n, &I18nService::languageChanged,
                this, &TextInspector::retranslateUi);
    retranslateUi();
    setVisible(false);
}

bool TextInspector::editingLayer(TextLayer** out) const
{
    if (!m_document || m_id.isNull())
        return false;
    Layer* layer = m_document->findLayer(m_id);
    if (!layer || layer->type() != LayerType::Text)
        return false;
    *out = static_cast<TextLayer*>(layer);
    return true;
}

void TextInspector::setDocument(Document* document)
{
    m_document = document;
    m_id = LayerId();
    refresh();
}

void TextInspector::setSelectedLayer(const LayerId& id)
{
    m_id = id;
    refresh();
}

void TextInspector::refresh()
{
    TextLayer* layer = nullptr;
    const bool isText = editingLayer(&layer);
    setVisible(isText);
    if (!isText)
        return;

    m_loading = true;
    m_content->setPlainText(layer->content);
    m_fontFamily->setCurrentText(layer->fontFamily);
    m_size->setValue(static_cast<int>(layer->sizePt));
    m_bold->setChecked(layer->bold);
    m_italic->setChecked(layer->italic);
    m_underline->setChecked(layer->underline);
    const QString hex = layer->color.name(QColor::HexRgb);
    m_color->setText(hex);
    m_color->setStyleSheet(QStringLiteral("background: %1;").arg(hex));
    m_align->setCurrentIndex(static_cast<int>(layer->align));

    const TextEffects& fx = layer->effects;
    m_outlineOn->setChecked(fx.outline.enabled);
    const QString outlineHex = fx.outline.color.name(QColor::HexRgb);
    m_outlineColor->setText(outlineHex);
    m_outlineColor->setStyleSheet(QStringLiteral("background: %1;").arg(outlineHex));
    m_outlineWidth->setValue(fx.outline.width);
    m_shadowOn->setChecked(fx.shadow.enabled);
    const QString shadowHex = fx.shadow.color.name(QColor::HexRgb);
    m_shadowColor->setText(shadowHex);
    m_shadowColor->setStyleSheet(QStringLiteral("background: %1;").arg(shadowHex));
    m_shadowX->setValue(fx.shadow.offsetX);
    m_shadowY->setValue(fx.shadow.offsetY);
    m_shadowBlur->setValue(fx.shadow.blur);

    const TextGradient& grad = fx.gradient;
    m_gradientOn->setChecked(grad.enabled);
    m_gradientType->setCurrentIndex(grad.type);
    const QString gradStartHex = grad.startColor.name(QColor::HexRgb);
    m_gradientStartColor->setText(gradStartHex);
    m_gradientStartColor->setStyleSheet(QStringLiteral("background: %1;").arg(gradStartHex));
    const QString gradEndHex = grad.endColor.name(QColor::HexRgb);
    m_gradientEndColor->setText(gradEndHex);
    m_gradientEndColor->setStyleSheet(QStringLiteral("background: %1;").arg(gradEndHex));
    m_gradientAngle->setValue(grad.angleDeg);

    m_tiltX->setValue(layer->transform.shearX);
    m_tiltY->setValue(layer->transform.shearY);
    m_loading = false;
}

void TextInspector::touch()
{
    if (m_document)
        m_document->touchLayer(m_id);
}

void TextInspector::buildUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(4);

    // 1. Seção Tipografia (Aberta por padrão)
    m_typographySection = new CollapsibleSection(QStringLiteral("Typography"), true, this);
    auto* typoForm = new QFormLayout;
    typoForm->setContentsMargins(4, 4, 4, 4);

    m_content = new QPlainTextEdit(this);
    m_content->setMaximumHeight(70);
    m_contentLabel = new QLabel(this);
    typoForm->addRow(m_contentLabel, m_content);

    m_fontFamily = new QFontComboBox(this);
    m_fontLabel = new QLabel(this);
    typoForm->addRow(m_fontLabel, m_fontFamily);

    m_size = new QSpinBox(this);
    m_size->setRange(4, 500);
    m_size->setValue(48);
    m_sizeLabel = new QLabel(this);
    typoForm->addRow(m_sizeLabel, m_size);

    m_bold = new QToolButton(this);
    m_bold->setCheckable(true);
    m_bold->setText(QStringLiteral("B"));
    m_bold->setStyleSheet(QStringLiteral("font-weight: bold;"));
    m_italic = new QToolButton(this);
    m_italic->setCheckable(true);
    m_italic->setText(QStringLiteral("I"));
    m_italic->setStyleSheet(QStringLiteral("font-style: italic;"));
    m_underline = new QToolButton(this);
    m_underline->setCheckable(true);
    m_underline->setText(QStringLiteral("U"));
    m_underline->setStyleSheet(QStringLiteral("text-decoration: underline;"));

    m_color = new QPushButton(this);

    m_styleLabel = new QLabel(this);
    auto* styleRow = new QHBoxLayout;
    styleRow->addWidget(m_bold);
    styleRow->addWidget(m_italic);
    styleRow->addWidget(m_underline);
    styleRow->addWidget(m_color);
    styleRow->addStretch();
    typoForm->addRow(m_styleLabel, styleRow);

    m_align = new QComboBox(this);
    m_alignLabel = new QLabel(this);
    typoForm->addRow(m_alignLabel, m_align);

    m_typographySection->setContentLayout(typoForm);
    layout->addWidget(m_typographySection);

    // 2. Seção Efeitos de Contorno e Sombra (Recolhida por padrão)
    m_effectsSection = new CollapsibleSection(QStringLiteral("Text Effects"), false, this);
    auto* fxLayout = new QVBoxLayout;
    fxLayout->setContentsMargins(4, 4, 4, 4);
    fxLayout->setSpacing(4);

    m_outlineOn = new QCheckBox(this);
    m_outlineColor = new QPushButton(this);
    m_outlineWidth = new QDoubleSpinBox(this);
    m_outlineWidth->setRange(0.5, 40.0);
    m_outlineWidth->setSingleStep(0.5);
    auto* outlineRow = new QHBoxLayout;
    outlineRow->addWidget(m_outlineOn);
    outlineRow->addWidget(m_outlineColor);
    outlineRow->addWidget(m_outlineWidth);
    outlineRow->addStretch();
    fxLayout->addLayout(outlineRow);

    m_shadowOn = new QCheckBox(this);
    m_shadowColor = new QPushButton(this);
    m_shadowX = new QDoubleSpinBox(this);
    m_shadowX->setRange(-200.0, 200.0);
    m_shadowY = new QDoubleSpinBox(this);
    m_shadowY->setRange(-200.0, 200.0);
    m_shadowBlur = new QDoubleSpinBox(this);
    m_shadowBlur->setRange(0.0, 60.0);
    m_shadowBlur->setSingleStep(1.0);
    auto* shadowRow = new QHBoxLayout;
    shadowRow->addWidget(m_shadowOn);
    shadowRow->addWidget(m_shadowColor);
    shadowRow->addWidget(m_shadowX);
    shadowRow->addWidget(m_shadowY);
    shadowRow->addWidget(m_shadowBlur);
    shadowRow->addStretch();
    fxLayout->addLayout(shadowRow);

    m_effectsSection->setContentLayout(fxLayout);
    layout->addWidget(m_effectsSection);

    // 3. Seção Gradiente de Texto (Recolhida por padrão)
    m_gradientSection = new CollapsibleSection(QStringLiteral("Gradient"), false, this);
    auto* gradLayout = new QVBoxLayout;
    gradLayout->setContentsMargins(4, 4, 4, 4);

    m_gradientOn = new QCheckBox(this);
    m_gradientType = new QComboBox(this);
    m_gradientType->addItem(QStringLiteral("Linear"), 0);
    m_gradientType->addItem(QStringLiteral("Radial"), 1);
    m_gradientStartColor = new QPushButton(this);
    m_gradientEndColor = new QPushButton(this);
    m_gradientAngle = new QDoubleSpinBox(this);
    m_gradientAngle->setRange(0.0, 360.0);
    m_gradientAngle->setSingleStep(5.0);
    m_gradientAngle->setSuffix(QStringLiteral("°"));

    auto* gradientRow = new QHBoxLayout;
    gradientRow->addWidget(m_gradientOn);
    gradientRow->addWidget(m_gradientType);
    gradientRow->addWidget(m_gradientStartColor);
    gradientRow->addWidget(m_gradientEndColor);
    gradientRow->addWidget(new QLabel(QStringLiteral("∠"), this));
    gradientRow->addWidget(m_gradientAngle);
    gradientRow->addStretch();
    gradLayout->addLayout(gradientRow);

    m_gradientSection->setContentLayout(gradLayout);
    layout->addWidget(m_gradientSection);

    // 4. Seção Perspectiva 3D (Recolhida por padrão)
    m_perspectiveSection = new CollapsibleSection(QStringLiteral("3D Tilt & Perspective"), false, this);
    auto* tiltLayout = new QVBoxLayout;
    tiltLayout->setContentsMargins(4, 4, 4, 4);

    m_perspectiveLabel = new QLabel(this);

    m_tiltX = new QDoubleSpinBox(this);
    m_tiltX->setRange(-2.0, 2.0);
    m_tiltX->setSingleStep(0.05);
    m_tiltX->setDecimals(2);

    m_tiltY = new QDoubleSpinBox(this);
    m_tiltY->setRange(-2.0, 2.0);
    m_tiltY->setSingleStep(0.05);
    m_tiltY->setDecimals(2);

    auto* tiltRow = new QHBoxLayout;
    tiltRow->addWidget(new QLabel(QStringLiteral("X:"), this));
    tiltRow->addWidget(m_tiltX);
    tiltRow->addWidget(new QLabel(QStringLiteral("Y:"), this));
    tiltRow->addWidget(m_tiltY);
    tiltRow->addStretch();
    tiltLayout->addLayout(tiltRow);

    m_perspectiveSection->setContentLayout(tiltLayout);
    layout->addWidget(m_perspectiveSection);

    layout->addStretch();

    connect(m_color, &QPushButton::clicked, this, [this] {
        TextLayer* layer = nullptr;
        if (!editingLayer(&layer)) return;
        const QColor chosen = QColorDialog::getColor(layer->color, this);
        if (!chosen.isValid()) return;
        layer->color = chosen;
        const QString hex = chosen.name(QColor::HexRgb);
        m_color->setText(hex);
        m_color->setStyleSheet(QStringLiteral("background: %1;").arg(hex));
        touch();
    });

    connect(m_outlineOn, &QCheckBox::toggled, this, [this](bool on) {
        TextLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        layer->effects.outline.enabled = on;
        touch();
    });
    connect(m_outlineColor, &QPushButton::clicked, this, [this] {
        TextLayer* layer = nullptr;
        if (!editingLayer(&layer)) return;
        const QColor chosen = QColorDialog::getColor(layer->effects.outline.color, this);
        if (!chosen.isValid()) return;
        layer->effects.outline.color = chosen;
        const QString hex = chosen.name(QColor::HexRgb);
        m_outlineColor->setText(hex);
        m_outlineColor->setStyleSheet(QStringLiteral("background: %1;").arg(hex));
        touch();
    });
    connect(m_outlineWidth, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this](double value) {
        TextLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        layer->effects.outline.width = value;
        touch();
    });
    connect(m_shadowOn, &QCheckBox::toggled, this, [this](bool on) {
        TextLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        layer->effects.shadow.enabled = on;
        touch();
    });
    connect(m_shadowColor, &QPushButton::clicked, this, [this] {
        TextLayer* layer = nullptr;
        if (!editingLayer(&layer)) return;
        const QColor chosen = QColorDialog::getColor(layer->effects.shadow.color, this);
        if (!chosen.isValid()) return;
        layer->effects.shadow.color = chosen;
        const QString hex = chosen.name(QColor::HexRgb);
        m_shadowColor->setText(hex);
        m_shadowColor->setStyleSheet(QStringLiteral("background: %1;").arg(hex));
        touch();
    });
    connect(m_shadowX, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this](double value) {
        TextLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        layer->effects.shadow.offsetX = value;
        touch();
    });
    connect(m_shadowY, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this](double value) {
        TextLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        layer->effects.shadow.offsetY = value;
        touch();
    });
    connect(m_shadowBlur, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this](double value) {
        TextLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        layer->effects.shadow.blur = value;
        touch();
    });
    connect(m_gradientOn, &QCheckBox::toggled, this, [this](bool on) {
        TextLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        layer->effects.gradient.enabled = on;
        touch();
    });
    connect(m_gradientType, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int index) {
        TextLayer* layer = nullptr;
        if (m_loading || index < 0 || !editingLayer(&layer)) return;
        layer->effects.gradient.type = index;
        touch();
    });
    connect(m_gradientStartColor, &QPushButton::clicked, this, [this] {
        TextLayer* layer = nullptr;
        if (!editingLayer(&layer)) return;
        const QColor chosen = QColorDialog::getColor(layer->effects.gradient.startColor, this);
        if (!chosen.isValid()) return;
        layer->effects.gradient.startColor = chosen;
        const QString hex = chosen.name(QColor::HexRgb);
        m_gradientStartColor->setText(hex);
        m_gradientStartColor->setStyleSheet(QStringLiteral("background: %1;").arg(hex));
        touch();
    });
    connect(m_gradientEndColor, &QPushButton::clicked, this, [this] {
        TextLayer* layer = nullptr;
        if (!editingLayer(&layer)) return;
        const QColor chosen = QColorDialog::getColor(layer->effects.gradient.endColor, this);
        if (!chosen.isValid()) return;
        layer->effects.gradient.endColor = chosen;
        const QString hex = chosen.name(QColor::HexRgb);
        m_gradientEndColor->setText(hex);
        m_gradientEndColor->setStyleSheet(QStringLiteral("background: %1;").arg(hex));
        touch();
    });
    connect(m_gradientAngle, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this](double value) {
        TextLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        layer->effects.gradient.angleDeg = value;
        touch();
    });
    connect(m_tiltX, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this](double value) {
        TextLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        layer->transform.shearX = value;
        touch();
    });
    connect(m_tiltY, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this](double value) {
        TextLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        layer->transform.shearY = value;
        touch();
    });
}

void TextInspector::retranslateUi()
{
    if (!m_i18n)
        return;

    if (m_typographySection)
        m_typographySection->setTitle(m_i18n->t("editor", "section.typography"));
    if (m_effectsSection)
        m_effectsSection->setTitle(m_i18n->t("editor", "section.textEffects"));
    if (m_gradientSection)
        m_gradientSection->setTitle(m_i18n->t("editor", "section.gradient"));
    if (m_perspectiveSection)
        m_perspectiveSection->setTitle(m_i18n->t("editor", "section.perspective"));

    m_contentLabel->setText(m_i18n->t("editor", "text.content"));
    m_fontLabel->setText(m_i18n->t("editor", "text.font"));
    m_sizeLabel->setText(m_i18n->t("editor", "text.size"));
    m_styleLabel->setText(m_i18n->t("editor", "text.style"));
    m_alignLabel->setText(m_i18n->t("editor", "text.align"));
    m_outlineOn->setText(m_i18n->t("editor", "effects.outline"));
    m_shadowOn->setText(m_i18n->t("editor", "effects.shadow"));
    m_gradientOn->setText(m_i18n ? m_i18n->t("editor", "effects.gradient") : QStringLiteral("Gradient"));
    m_perspectiveLabel->setText(m_i18n ? m_i18n->t("editor", "effects.perspective") : QStringLiteral("3D Tilt / Perspective"));

    m_loading = true;
    m_align->clear();
    m_align->addItem(m_i18n->t("editor", "text.align.left"));
    m_align->addItem(m_i18n->t("editor", "text.align.center"));
    m_align->addItem(m_i18n->t("editor", "text.align.right"));
    TextLayer* layer = nullptr;
    if (editingLayer(&layer))
        m_align->setCurrentIndex(static_cast<int>(layer->align));
    m_loading = false;
}

} // namespace cc
