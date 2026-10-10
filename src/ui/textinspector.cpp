#include "textinspector.h"
#include "collapsiblesection.h"
#include "localization/i18nservice.h"
#include "core/history/CommandStack.h"
#include "core/history/DocumentCommands.h"

#include <QColorDialog>
#include <QFileDialog>
#include <QFileInfo>
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
#include <QTimer>
#include <QEvent>

namespace cc {

TextInspector::TextInspector(I18nService* i18n, Document* document,
                             CommandStack* history, QWidget* parent)
    : QWidget(parent)
    , m_i18n(i18n)
    , m_document(document)
    , m_history(history)
{
    m_textDebounceTimer = new QTimer(this);
    m_textDebounceTimer->setSingleShot(true);
    m_textDebounceTimer->setInterval(500);
    connect(m_textDebounceTimer, &QTimer::timeout,
            this, &TextInspector::commitPendingTextChange);

    buildUi();

    if (m_i18n)
        connect(m_i18n, &I18nService::languageChanged,
                this, &TextInspector::retranslateUi);
    retranslateUi();
    setVisible(false);
}

TextInspector::~TextInspector()
{
    commitPendingTextChange();
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
    commitPendingTextChange();
    m_document = document;
    m_id = LayerId();
    refresh();
}

void TextInspector::setSelectedLayer(const LayerId& id)
{
    if (m_id != id) {
        commitPendingTextChange();
    }
    m_id = id;
    refresh();
}

void TextInspector::commitPendingTextChange()
{
    if (m_textDebounceTimer && m_textDebounceTimer->isActive())
        m_textDebounceTimer->stop();

    if (!m_isTypingText)
        return;

    m_isTypingText = false;

    TextLayer* layer = nullptr;
    if (!editingLayer(&layer))
        return;

    const QString newContent = m_content ? m_content->toPlainText() : QString();
    if (newContent == m_initialTextContent)
        return;

    if (m_history && m_document) {
        m_history->execute(std::make_unique<LayerPropertyCommand<QString>>(
            *m_document, m_id, QStringLiteral("layer.text"),
            &Document::setLayerTextContent, m_initialTextContent, newContent));
    }
    m_initialTextContent = newContent;
}

bool TextInspector::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_content && event->type() == QEvent::FocusOut) {
        commitPendingTextChange();
    }
    return QWidget::eventFilter(watched, event);
}

void TextInspector::refresh()
{
    if (m_isTypingText && (!m_content || !m_content->hasFocus())) {
        commitPendingTextChange();
    } else if (m_textDebounceTimer && m_textDebounceTimer->isActive()) {
        m_textDebounceTimer->stop();
        m_isTypingText = false;
    }

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
    m_letterSpacing->setValue(layer->letterSpacingPx);
    m_lineHeight->setValue(layer->lineHeightMult > 0.0 ? layer->lineHeightMult : 1.0);

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

    m_hollowOn->setChecked(fx.hollow);
    m_knockoutOn->setChecked(fx.knockout);

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

    const TextTextureFill& tex = fx.texture;
    m_textureOn->setChecked(tex.enabled);
    m_textureOpacity->setValue(tex.opacity * 100.0);
    if (!tex.assetId.isNull()) {
        m_textureNameLabel->setText(QStringLiteral("Textura ativa"));
    } else {
        m_textureNameLabel->setText(QStringLiteral("Nenhuma imagem"));
    }

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
    m_content->installEventFilter(this);
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

    m_letterSpacing = new QDoubleSpinBox(this);
    m_letterSpacing->setRange(-50.0, 200.0);
    m_letterSpacing->setSingleStep(0.5);
    m_letterSpacing->setDecimals(1);
    m_letterSpacing->setSuffix(QStringLiteral(" px"));
    m_letterSpacingLabel = new QLabel(this);
    typoForm->addRow(m_letterSpacingLabel, m_letterSpacing);

    m_lineHeight = new QDoubleSpinBox(this);
    m_lineHeight->setRange(0.2, 5.0);
    m_lineHeight->setSingleStep(0.1);
    m_lineHeight->setDecimals(2);
    m_lineHeight->setValue(1.0);
    m_lineHeightLabel = new QLabel(this);
    typoForm->addRow(m_lineHeightLabel, m_lineHeight);

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

    m_hollowOn = new QCheckBox(this);
    m_knockoutOn = new QCheckBox(this);
    auto* cutRow = new QHBoxLayout;
    cutRow->addWidget(m_hollowOn);
    cutRow->addWidget(m_knockoutOn);
    cutRow->addStretch();
    fxLayout->addLayout(cutRow);

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

    // 4. Seção Textura / Imagem de Preenchimento (Recolhida por padrão)
    m_textureSection = new CollapsibleSection(QStringLiteral("Texture / Image Fill"), false, this);
    auto* texLayout = new QVBoxLayout;
    texLayout->setContentsMargins(4, 4, 4, 4);

    m_textureOn = new QCheckBox(this);
    m_textureChooseBtn = new QPushButton(this);
    m_textureClearBtn = new QPushButton(this);
    m_textureNameLabel = new QLabel(QStringLiteral("Nenhuma imagem"), this);
    m_textureNameLabel->setStyleSheet(QStringLiteral("color: #888888; font-style: italic;"));

    auto* texRow1 = new QHBoxLayout;
    texRow1->addWidget(m_textureOn);
    texRow1->addWidget(m_textureChooseBtn);
    texRow1->addWidget(m_textureClearBtn);
    texRow1->addWidget(m_textureNameLabel);
    texRow1->addStretch();
    texLayout->addLayout(texRow1);

    m_textureOpacityLabel = new QLabel(this);
    m_textureOpacity = new QDoubleSpinBox(this);
    m_textureOpacity->setRange(0.0, 100.0);
    m_textureOpacity->setValue(100.0);
    m_textureOpacity->setSingleStep(5.0);
    m_textureOpacity->setSuffix(QStringLiteral("%"));

    auto* texRow2 = new QHBoxLayout;
    texRow2->addWidget(m_textureOpacityLabel);
    texRow2->addWidget(m_textureOpacity);
    texRow2->addStretch();
    texLayout->addLayout(texRow2);

    m_textureSection->setContentLayout(texLayout);
    layout->addWidget(m_textureSection);

    // 5. Seção Perspectiva 3D (Recolhida por padrão)
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

    // Helper lambda to apply text effects with undo/redo
    auto applyEffects = [this](auto mutator) {
        TextLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        TextEffects oldFx = layer->effects;
        TextEffects newFx = oldFx;
        mutator(newFx);
        if (oldFx == newFx) return;

        if (m_history && m_document) {
            m_history->execute(std::make_unique<SetLayerTextEffectsCommand>(
                *m_document, m_id, oldFx, newFx));
        } else if (m_document) {
            m_document->setLayerTextEffects(m_id, newFx);
        }
    };

    // --- Typography controls ---
    connect(m_content, &QPlainTextEdit::textChanged, this, [this] {
        TextLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        const QString newContent = m_content->toPlainText();
        if (layer->content == newContent) return;

        if (!m_isTypingText) {
            m_isTypingText = true;
            m_initialTextContent = layer->content;
        }

        // Live update on document/canvas without spamming history
        if (m_document) {
            m_document->setLayerTextContent(m_id, newContent);
        }

        if (m_textDebounceTimer) {
            m_textDebounceTimer->start(500);
        }
    });
    connect(m_fontFamily, &QFontComboBox::currentTextChanged, this, [this](const QString& family) {
        TextLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        if (layer->fontFamily == family) return;

        if (m_history && m_document) {
            m_history->execute(std::make_unique<SetTextFontFamilyCommand>(
                *m_document, m_id, QStringLiteral("text.fontFamily"),
                &Document::setLayerFontFamily, layer->fontFamily, family));
        } else if (m_document) {
            m_document->setLayerFontFamily(m_id, family);
        }
    });
    connect(m_size, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
        TextLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        const double newSize = static_cast<double>(value);
        if (qFuzzyCompare(layer->sizePt, newSize)) return;

        if (m_history && m_document) {
            m_history->execute(std::make_unique<SetTextSizeCommand>(
                *m_document, m_id, QStringLiteral("text.size"),
                &Document::setLayerTextSize, layer->sizePt, newSize));
        } else if (m_document) {
            m_document->setLayerTextSize(m_id, newSize);
        }
    });
    connect(m_bold, &QToolButton::toggled, this, [this](bool on) {
        TextLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        if (layer->bold == on) return;

        if (m_history && m_document) {
            m_history->execute(std::make_unique<SetTextBoldCommand>(
                *m_document, m_id, QStringLiteral("text.bold"),
                &Document::setLayerTextBold, layer->bold, on));
        } else if (m_document) {
            m_document->setLayerTextBold(m_id, on);
        }
    });
    connect(m_italic, &QToolButton::toggled, this, [this](bool on) {
        TextLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        if (layer->italic == on) return;

        if (m_history && m_document) {
            m_history->execute(std::make_unique<SetTextItalicCommand>(
                *m_document, m_id, QStringLiteral("text.italic"),
                &Document::setLayerTextItalic, layer->italic, on));
        } else if (m_document) {
            m_document->setLayerTextItalic(m_id, on);
        }
    });
    connect(m_underline, &QToolButton::toggled, this, [this](bool on) {
        TextLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        if (layer->underline == on) return;

        if (m_history && m_document) {
            m_history->execute(std::make_unique<SetTextUnderlineCommand>(
                *m_document, m_id, QStringLiteral("text.underline"),
                &Document::setLayerTextUnderline, layer->underline, on));
        } else if (m_document) {
            m_document->setLayerTextUnderline(m_id, on);
        }
    });
    connect(m_align, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int idx) {
        TextLayer* layer = nullptr;
        if (m_loading || idx < 0 || !editingLayer(&layer)) return;
        const auto newAlign = static_cast<TextAlignment>(idx);
        if (layer->align == newAlign) return;

        if (m_history && m_document) {
            m_history->execute(std::make_unique<SetTextAlignCommand>(
                *m_document, m_id, QStringLiteral("text.align"),
                &Document::setLayerTextAlign, layer->align, newAlign));
        } else if (m_document) {
            m_document->setLayerTextAlign(m_id, newAlign);
        }
    });

    connect(m_letterSpacing, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        TextLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        if (qFuzzyCompare(layer->letterSpacingPx, val)) return;

        if (m_history && m_document) {
            m_history->execute(std::make_unique<SetTextLetterSpacingCommand>(
                *m_document, m_id, QStringLiteral("text.letterSpacing"),
                &Document::setLayerLetterSpacing, layer->letterSpacingPx, val));
        } else if (m_document) {
            m_document->setLayerLetterSpacing(m_id, val);
        }
    });

    connect(m_lineHeight, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double val) {
        TextLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        if (qFuzzyCompare(layer->lineHeightMult, val)) return;

        if (m_history && m_document) {
            m_history->execute(std::make_unique<SetTextLineHeightCommand>(
                *m_document, m_id, QStringLiteral("text.lineHeight"),
                &Document::setLayerLineHeight, layer->lineHeightMult, val));
        } else if (m_document) {
            m_document->setLayerLineHeight(m_id, val);
        }
    });

    connect(m_color, &QPushButton::clicked, this, [this] {
        TextLayer* layer = nullptr;
        if (!editingLayer(&layer)) return;
        const QColor chosen = QColorDialog::getColor(layer->color, this);
        if (!chosen.isValid()) return;

        const QString hex = chosen.name(QColor::HexRgb);
        m_color->setText(hex);
        m_color->setStyleSheet(QStringLiteral("background: %1;").arg(hex));

        if (m_history && m_document) {
            m_history->execute(std::make_unique<SetTextColorCommand>(
                *m_document, m_id, QStringLiteral("text.color"),
                &Document::setLayerTextColor, layer->color, chosen));
        } else if (m_document) {
            m_document->setLayerTextColor(m_id, chosen);
        }
    });

    connect(m_outlineOn, &QCheckBox::toggled, this, [applyEffects](bool on) {
        applyEffects([on](TextEffects& fx) { fx.outline.enabled = on; });
    });
    connect(m_outlineColor, &QPushButton::clicked, this, [this, applyEffects] {
        TextLayer* layer = nullptr;
        if (!editingLayer(&layer)) return;
        const QColor chosen = QColorDialog::getColor(layer->effects.outline.color, this);
        if (!chosen.isValid()) return;
        const QString hex = chosen.name(QColor::HexRgb);
        m_outlineColor->setText(hex);
        m_outlineColor->setStyleSheet(QStringLiteral("background: %1;").arg(hex));
        applyEffects([chosen](TextEffects& fx) { fx.outline.color = chosen; });
    });
    connect(m_outlineWidth, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [applyEffects](double value) {
        applyEffects([value](TextEffects& fx) { fx.outline.width = value; });
    });
    connect(m_shadowOn, &QCheckBox::toggled, this, [applyEffects](bool on) {
        applyEffects([on](TextEffects& fx) { fx.shadow.enabled = on; });
    });
    connect(m_shadowColor, &QPushButton::clicked, this, [this, applyEffects] {
        TextLayer* layer = nullptr;
        if (!editingLayer(&layer)) return;
        const QColor chosen = QColorDialog::getColor(layer->effects.shadow.color, this);
        if (!chosen.isValid()) return;
        const QString hex = chosen.name(QColor::HexRgb);
        m_shadowColor->setText(hex);
        m_shadowColor->setStyleSheet(QStringLiteral("background: %1;").arg(hex));
        applyEffects([chosen](TextEffects& fx) { fx.shadow.color = chosen; });
    });
    connect(m_shadowX, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [applyEffects](double value) {
        applyEffects([value](TextEffects& fx) { fx.shadow.offsetX = value; });
    });
    connect(m_shadowY, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [applyEffects](double value) {
        applyEffects([value](TextEffects& fx) { fx.shadow.offsetY = value; });
    });
    connect(m_shadowBlur, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [applyEffects](double value) {
        applyEffects([value](TextEffects& fx) { fx.shadow.blur = value; });
    });
    connect(m_hollowOn, &QCheckBox::toggled, this, [applyEffects](bool on) {
        applyEffects([on](TextEffects& fx) { fx.hollow = on; });
    });
    connect(m_knockoutOn, &QCheckBox::toggled, this, [applyEffects](bool on) {
        applyEffects([on](TextEffects& fx) { fx.knockout = on; });
    });
    connect(m_gradientOn, &QCheckBox::toggled, this, [applyEffects](bool on) {
        applyEffects([on](TextEffects& fx) { fx.gradient.enabled = on; });
    });
    connect(m_gradientType, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [applyEffects](int index) {
        if (index < 0) return;
        applyEffects([index](TextEffects& fx) { fx.gradient.type = index; });
    });
    connect(m_gradientStartColor, &QPushButton::clicked, this, [this, applyEffects] {
        TextLayer* layer = nullptr;
        if (!editingLayer(&layer)) return;
        const QColor chosen = QColorDialog::getColor(layer->effects.gradient.startColor, this);
        if (!chosen.isValid()) return;
        const QString hex = chosen.name(QColor::HexRgb);
        m_gradientStartColor->setText(hex);
        m_gradientStartColor->setStyleSheet(QStringLiteral("background: %1;").arg(hex));
        applyEffects([chosen](TextEffects& fx) { fx.gradient.startColor = chosen; });
    });
    connect(m_gradientEndColor, &QPushButton::clicked, this, [this, applyEffects] {
        TextLayer* layer = nullptr;
        if (!editingLayer(&layer)) return;
        const QColor chosen = QColorDialog::getColor(layer->effects.gradient.endColor, this);
        if (!chosen.isValid()) return;
        const QString hex = chosen.name(QColor::HexRgb);
        m_gradientEndColor->setText(hex);
        m_gradientEndColor->setStyleSheet(QStringLiteral("background: %1;").arg(hex));
        applyEffects([chosen](TextEffects& fx) { fx.gradient.endColor = chosen; });
    });
    connect(m_gradientAngle, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [applyEffects](double value) {
        applyEffects([value](TextEffects& fx) { fx.gradient.angleDeg = value; });
    });

    connect(m_textureOn, &QCheckBox::toggled, this, [applyEffects](bool on) {
        applyEffects([on](TextEffects& fx) { fx.texture.enabled = on; });
    });
    connect(m_textureChooseBtn, &QPushButton::clicked, this, [this, applyEffects] {
        TextLayer* layer = nullptr;
        if (!editingLayer(&layer) || !m_document) return;
        const QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("Selecionar Imagem / Textura"), QString(),
            QStringLiteral("Imagens (*.png *.jpg *.jpeg *.webp *.bmp)"));
        if (path.isEmpty()) return;
        const QImage img(path);
        if (img.isNull()) return;

        const LayerId assetId = m_document->assets().addImage(img);
        applyEffects([assetId](TextEffects& fx) {
            fx.texture.enabled = true;
            fx.texture.assetId = assetId;
        });
        m_textureOn->setChecked(true);
        m_textureNameLabel->setText(QFileInfo(path).fileName());
    });
    connect(m_textureClearBtn, &QPushButton::clicked, this, [this, applyEffects] {
        applyEffects([](TextEffects& fx) {
            fx.texture.enabled = false;
            fx.texture.assetId = LayerId();
        });
        m_textureOn->setChecked(false);
        m_textureNameLabel->setText(QStringLiteral("Nenhuma imagem"));
    });
    connect(m_textureOpacity, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [applyEffects](double value) {
        applyEffects([value](TextEffects& fx) { fx.texture.opacity = value / 100.0; });
    });

    connect(m_tiltX, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this](double value) {
        TextLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        AffineTransform oldT = layer->transform;
        AffineTransform newT = oldT;
        newT.shearX = value;
        if (m_history && m_document) {
            m_history->execute(std::make_unique<SetLayerTransformCommand>(
                *m_document, m_id, oldT, newT));
        } else if (m_document) {
            m_document->setLayerTransform(m_id, newT);
        }
    });
    connect(m_tiltY, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this](double value) {
        TextLayer* layer = nullptr;
        if (m_loading || !editingLayer(&layer)) return;
        AffineTransform oldT = layer->transform;
        AffineTransform newT = oldT;
        newT.shearY = value;
        if (m_history && m_document) {
            m_history->execute(std::make_unique<SetLayerTransformCommand>(
                *m_document, m_id, oldT, newT));
        } else if (m_document) {
            m_document->setLayerTransform(m_id, newT);
        }
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
    if (m_textureSection)
        m_textureSection->setTitle(m_i18n ? m_i18n->t("editor", "section.textureFill") : QStringLiteral("Texture / Image Fill"));
    if (m_perspectiveSection)
        m_perspectiveSection->setTitle(m_i18n->t("editor", "section.perspective"));

    m_contentLabel->setText(m_i18n->t("editor", "text.content"));
    m_fontLabel->setText(m_i18n->t("editor", "text.font"));
    m_sizeLabel->setText(m_i18n->t("editor", "text.size"));
    m_styleLabel->setText(m_i18n->t("editor", "text.style"));
    m_alignLabel->setText(m_i18n->t("editor", "text.align"));
    m_letterSpacingLabel->setText(m_i18n ? m_i18n->t("editor", "text.letterSpacing") : QStringLiteral("Tracking (Spacing)"));
    m_lineHeightLabel->setText(m_i18n ? m_i18n->t("editor", "text.lineHeight") : QStringLiteral("Leading (Line Height)"));
    m_outlineOn->setText(m_i18n->t("editor", "effects.outline"));
    m_shadowOn->setText(m_i18n->t("editor", "effects.shadow"));
    m_hollowOn->setText(m_i18n ? m_i18n->t("editor", "effects.hollow") : QStringLiteral("Interior Vazado"));
    m_knockoutOn->setText(m_i18n ? m_i18n->t("editor", "effects.knockout") : QStringLiteral("Recorte / Chroma Cut"));
    m_gradientOn->setText(m_i18n ? m_i18n->t("editor", "effects.gradient") : QStringLiteral("Gradient"));
    m_textureOn->setText(m_i18n ? m_i18n->t("editor", "effects.texture") : QStringLiteral("Texture Fill"));
    m_textureChooseBtn->setText(m_i18n ? m_i18n->t("editor", "effects.textureChoose") : QStringLiteral("Choose..."));
    m_textureClearBtn->setText(m_i18n ? m_i18n->t("editor", "effects.textureClear") : QStringLiteral("Clear"));
    m_textureOpacityLabel->setText(m_i18n ? m_i18n->t("editor", "effects.textureOpacity") : QStringLiteral("Opacity:"));
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
