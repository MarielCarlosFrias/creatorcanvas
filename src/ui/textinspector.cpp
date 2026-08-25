#include "textinspector.h"

#include "localization/i18nservice.h"

#include <QColorDialog>
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
