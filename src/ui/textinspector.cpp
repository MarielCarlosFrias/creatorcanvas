#include "textinspector.h"

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

    auto* group = new QGroupBox(this);
    auto* form = new QFormLayout(group);

    m_content = new QPlainTextEdit(group);
    m_content->setMaximumHeight(80);
    m_contentLabel = new QLabel(group);
    form->addRow(m_contentLabel, m_content);

    m_fontFamily = new QFontComboBox(group);
    m_fontLabel = new QLabel(group);
    form->addRow(m_fontLabel, m_fontFamily);

    m_size = new QSpinBox(group);
    m_size->setRange(4, 500);
    m_size->setValue(48);
    m_sizeLabel = new QLabel(group);
    form->addRow(m_sizeLabel, m_size);

    m_bold = new QToolButton(group);
    m_bold->setCheckable(true);
    m_bold->setText(QStringLiteral("B"));
    m_bold->setStyleSheet(QStringLiteral("font-weight: bold;"));
    m_italic = new QToolButton(group);
    m_italic->setCheckable(true);
    m_italic->setText(QStringLiteral("I"));
    m_italic->setStyleSheet(QStringLiteral("font-style: italic;"));
    m_underline = new QToolButton(group);
    m_underline->setCheckable(true);
    m_underline->setText(QStringLiteral("U"));
    m_underline->setStyleSheet(QStringLiteral("text-decoration: underline;"));

    m_color = new QPushButton(group);

    m_styleLabel = new QLabel(group);
    auto* styleRow = new QHBoxLayout;
    styleRow->addWidget(m_bold);
    styleRow->addWidget(m_italic);
    styleRow->addWidget(m_underline);
    styleRow->addWidget(m_color);
    styleRow->addStretch();
    form->addRow(m_styleLabel, styleRow);

    m_align = new QComboBox(group);
    m_alignLabel = new QLabel(group);
    form->addRow(m_alignLabel, m_align);

    layout->addWidget(group);
    layout->addStretch();

    connect(m_content, &QPlainTextEdit::textChanged, this, [this] {
        TextLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer))
            return;
        layer->content = m_content->toPlainText();
        touch();
    });
    connect(m_fontFamily, &QFontComboBox::currentFontChanged, this,
            [this](const QFont& font) {
        TextLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer))
            return;
        layer->fontFamily = font.family();
        touch();
    });
    connect(m_size, qOverload<int>(&QSpinBox::valueChanged), this,
            [this](int value) {
        TextLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer))
            return;
        layer->sizePt = value;
        touch();
    });
    connect(m_bold, &QToolButton::toggled, this, [this](bool on) {
        TextLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer))
            return;
        layer->bold = on;
        touch();
    });
    connect(m_italic, &QToolButton::toggled, this, [this](bool on) {
        TextLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer))
            return;
        layer->italic = on;
        touch();
    });
    connect(m_underline, &QToolButton::toggled, this, [this](bool on) {
        TextLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer))
            return;
        layer->underline = on;
        touch();
    });
    connect(m_color, &QPushButton::clicked, this, [this] {
        TextLayer* layer = nullptr;
        if (!editingLayer(&layer))
            return;
        const QColor chosen = QColorDialog::getColor(layer->color, this);
        if (!chosen.isValid())
            return;
        layer->color = chosen;
        const QString hex = chosen.name(QColor::HexRgb);
        m_color->setText(hex);
        m_color->setStyleSheet(QStringLiteral("background: %1;").arg(hex));
        touch();
    });
    connect(m_align, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int index) {
        TextLayer* layer = nullptr;
        if (m_loading || index < 0 || !editingLayer(&layer))
            return;
        layer->align = static_cast<TextAlignment>(index);
        touch();
    });
    m_effectsLabel = new QLabel(group);
    form->addRow(m_effectsLabel);

    m_outlineOn = new QCheckBox(group);
    m_outlineColor = new QPushButton(group);
    m_outlineWidth = new QDoubleSpinBox(group);
    m_outlineWidth->setRange(0.5, 40.0);
    m_outlineWidth->setSingleStep(0.5);
    auto* outlineRow = new QHBoxLayout;
    outlineRow->addWidget(m_outlineOn);
    outlineRow->addWidget(m_outlineColor);
    outlineRow->addWidget(m_outlineWidth);
    outlineRow->addStretch();
    form->addRow(QString(), outlineRow);

    m_shadowOn = new QCheckBox(group);
    m_shadowColor = new QPushButton(group);
    m_shadowX = new QDoubleSpinBox(group);
    m_shadowX->setRange(-200.0, 200.0);
    m_shadowY = new QDoubleSpinBox(group);
    m_shadowY->setRange(-200.0, 200.0);
    m_shadowBlur = new QDoubleSpinBox(group);
    m_shadowBlur->setRange(0.0, 60.0);
    m_shadowBlur->setSingleStep(1.0);
    auto* shadowRow = new QHBoxLayout;
    shadowRow->addWidget(m_shadowOn);
    shadowRow->addWidget(m_shadowColor);
    shadowRow->addWidget(m_shadowX);
    shadowRow->addWidget(m_shadowY);
    shadowRow->addWidget(m_shadowBlur);
    shadowRow->addStretch();
    form->addRow(QString(), shadowRow);

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
}

void TextInspector::retranslateUi()
{
    if (!m_i18n)
        return;

    m_contentLabel->setText(m_i18n->t("editor", "text.content"));
    m_fontLabel->setText(m_i18n->t("editor", "text.font"));
    m_sizeLabel->setText(m_i18n->t("editor", "text.size"));
    m_styleLabel->setText(m_i18n->t("editor", "text.style"));
    m_alignLabel->setText(m_i18n->t("editor", "text.align"));
    m_effectsLabel->setText(m_i18n->t("editor", "effects.title"));
    m_outlineOn->setText(m_i18n->t("editor", "effects.outline"));
    m_shadowOn->setText(m_i18n->t("editor", "effects.shadow"));

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
