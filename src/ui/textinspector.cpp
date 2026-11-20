#include "textinspector.h"

#include "services/textstylestore.h"
#include "core/commands/TextPathCommands.h"
#include "collapsiblesection.h"
#include "localization/i18nservice.h"
#include "core/history/CommandStack.h"
#include "core/history/DocumentCommands.h"

#include <QColorDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QMessageBox>
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
#include <QScrollArea>
#include <QScrollBar>
#include <QFrame>
#include <QTimer>
#include <QEvent>

namespace cc {

static QString toTitleCase(const QString& str)
{
    QString result = str;
    bool newWord = true;
    for (int i = 0; i < result.size(); ++i) {
        if (result[i].isSpace() || result[i].isPunct()) {
            newWord = true;
        } else if (newWord) {
            result[i] = result[i].toUpper();
            newWord = false;
        } else {
            result[i] = result[i].toLower();
        }
    }
    return result;
}

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

QSize TextInspector::minimumSizeHint() const
{
    const QSize fallback = QWidget::minimumSizeHint();
    const auto* scroll = findChild<QScrollArea*>();
    if (!scroll || !scroll->widget())
        return fallback;

    // QScrollArea otherwise reports only its small default hint, hiding the
    // real width of translated forms and effect rows from the dock layout.
    // Reserve the vertical bar as well: it is normally needed by this panel
    // and must not cover the right-hand controls when it appears.
    const int contentWidth = scroll->widget()->minimumSizeHint().width();
    const int scrollBarWidth = scroll->verticalScrollBar()->sizeHint().width();
    const int frameWidth = 2 * scroll->frameWidth();
    return {qMax(fallback.width(), contentWidth + scrollBarWidth + frameWidth),
            fallback.height()};
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

    const TextBadge& badge = fx.badge;
    m_badgeOn->setChecked(badge.enabled);
    const QString badgeHex = badge.color.name(QColor::HexRgb);
    m_badgeColor->setText(badgeHex);
    m_badgeColor->setStyleSheet(QStringLiteral("background: %1;").arg(badgeHex));
    m_badgePaddingX->setValue(badge.paddingX);
    m_badgePaddingY->setValue(badge.paddingY);
    m_badgeRadius->setValue(badge.cornerRadius);

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
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    auto* content = new QWidget(this);
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(4);

    // 0. Presets Rápidos de Thumbnail / Estilos em 1 Clique
    m_quickPresetsSection = new CollapsibleSection(QStringLiteral("Presets Rápidos (Thumbnails)"), true, this);
    auto* presetsLayout = new QGridLayout;
    presetsLayout->setContentsMargins(4, 4, 4, 4);
    presetsLayout->setSpacing(4);

    auto makePresetBtn = [this](const QString& label, const QString& styleSheet, const QString& tooltip) {
        auto* btn = new QPushButton(label, this);
        btn->setToolTip(tooltip);
        btn->setStyleSheet(styleSheet);
        return btn;
    };

    auto* btnYt = makePresetBtn(QStringLiteral("🟡 Alert"),
        QStringLiteral("background: #ffe600; color: #000; font-weight: bold; border-radius: 4px; padding: 4px;"),
        QStringLiteral("YouTube Alert (Amarelo, contorno preto e sombra forte)"));
    auto* btnRed = makePresetBtn(QStringLiteral("🔴 Viral"),
        QStringLiteral("background: #e50914; color: #fff; font-weight: bold; border-radius: 4px; padding: 4px;"),
        QStringLiteral("Viral Red (Texto branco sobre tarja vermelha estilo YouTube)"));
    auto* btnNeon = makePresetBtn(QStringLiteral("🟣 Neon"),
        QStringLiteral("background: #111; color: #00ffff; font-weight: bold; border: 1px solid #ff007f; border-radius: 4px; padding: 4px;"),
        QStringLiteral("Cyberpunk Neon (Ciano elétrico com contorno e brilho rosa neon)"));
    auto* btnClean = makePresetBtn(QStringLiteral("⚪ Clean"),
        QStringLiteral("background: #222; color: #fff; border-radius: 10px; padding: 4px;"),
        QStringLiteral("Clean Minimal (Pílula escura moderna)"));
    auto* btnGold = makePresetBtn(QStringLiteral("🏆 Gold"),
        QStringLiteral("background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #ffd700, stop:1 #d48800); color: #000; font-weight: bold; border-radius: 4px; padding: 4px;"),
        QStringLiteral("Gold Luxury (Gradiente dourado premium e contorno)"));

    presetsLayout->addWidget(btnYt, 0, 0);
    presetsLayout->addWidget(btnRed, 0, 1);
    presetsLayout->addWidget(btnNeon, 0, 2);
    presetsLayout->addWidget(btnClean, 1, 0);
    presetsLayout->addWidget(btnGold, 1, 1);

    auto applyQuickPreset = [this](int type) {
        TextLayer* layer = nullptr;
        if (!editingLayer(&layer)) return;
        TextStyle s;
        s.fontFamily = layer->fontFamily;
        s.sizePt = layer->sizePt;
        s.letterSpacingPx = layer->letterSpacingPx;
        s.lineHeightMult = layer->lineHeightMult;

        if (type == 1) { // YouTube Alert
            s.bold = true;
            s.color = QColor(255, 230, 0);
            s.effects.outline.enabled = true;
            s.effects.outline.color = QColor(0, 0, 0);
            s.effects.outline.width = 6.0;
            s.effects.shadow.enabled = true;
            s.effects.shadow.color = QColor(0, 0, 0);
            s.effects.shadow.offsetX = 5.0;
            s.effects.shadow.offsetY = 5.0;
            s.effects.shadow.blur = 0.0;
            s.effects.badge.enabled = false;
        } else if (type == 2) { // Viral Red
            s.bold = true;
            s.color = QColor(255, 255, 255);
            s.effects.outline.enabled = true;
            s.effects.outline.color = QColor(183, 28, 28);
            s.effects.outline.width = 3.0;
            s.effects.shadow.enabled = true;
            s.effects.shadow.color = QColor(0, 0, 0, 180);
            s.effects.shadow.offsetX = 3.0;
            s.effects.shadow.offsetY = 3.0;
            s.effects.shadow.blur = 6.0;
            s.effects.badge.enabled = true;
            s.effects.badge.color = QColor(229, 9, 20);
            s.effects.badge.paddingX = 16.0;
            s.effects.badge.paddingY = 8.0;
            s.effects.badge.cornerRadius = 6.0;
        } else if (type == 3) { // Cyberpunk Neon
            s.bold = true;
            s.color = QColor(0, 255, 255);
            s.effects.outline.enabled = true;
            s.effects.outline.color = QColor(255, 0, 127);
            s.effects.outline.width = 3.0;
            s.effects.shadow.enabled = true;
            s.effects.shadow.color = QColor(255, 0, 127);
            s.effects.shadow.offsetX = 0.0;
            s.effects.shadow.offsetY = 0.0;
            s.effects.shadow.blur = 16.0;
            s.effects.badge.enabled = false;
        } else if (type == 4) { // Clean Minimal
            s.bold = false;
            s.color = QColor(255, 255, 255);
            s.effects.outline.enabled = false;
            s.effects.shadow.enabled = false;
            s.effects.badge.enabled = true;
            s.effects.badge.color = QColor(25, 25, 28, 220);
            s.effects.badge.paddingX = 14.0;
            s.effects.badge.paddingY = 6.0;
            s.effects.badge.cornerRadius = 12.0;
        } else if (type == 5) { // Gold Luxury
            s.bold = true;
            s.color = QColor(255, 215, 0);
            s.effects.gradient.enabled = true;
            s.effects.gradient.type = 0; // Linear
            s.effects.gradient.startColor = QColor(255, 243, 161);
            s.effects.gradient.endColor = QColor(212, 136, 0);
            s.effects.gradient.angleDeg = 45.0;
            s.effects.outline.enabled = true;
            s.effects.outline.color = QColor(62, 39, 35);
            s.effects.outline.width = 2.0;
            s.effects.shadow.enabled = true;
            s.effects.shadow.color = QColor(0, 0, 0, 200);
            s.effects.shadow.offsetX = 3.0;
            s.effects.shadow.offsetY = 4.0;
            s.effects.shadow.blur = 8.0;
            s.effects.badge.enabled = false;
        }
        applyTextStyle(s);
    };

    connect(btnYt, &QPushButton::clicked, this, [applyQuickPreset] { applyQuickPreset(1); });
    connect(btnRed, &QPushButton::clicked, this, [applyQuickPreset] { applyQuickPreset(2); });
    connect(btnNeon, &QPushButton::clicked, this, [applyQuickPreset] { applyQuickPreset(3); });
    connect(btnClean, &QPushButton::clicked, this, [applyQuickPreset] { applyQuickPreset(4); });
    connect(btnGold, &QPushButton::clicked, this, [applyQuickPreset] { applyQuickPreset(5); });

    m_quickPresetsSection->setContentLayout(presetsLayout);
    layout->addWidget(m_quickPresetsSection);

    // 1. Seção Tipografia (Aberta por padrão)
    m_typographySection = new CollapsibleSection(QStringLiteral("Typography"), true, this);
    auto* typoForm = new QFormLayout;
    typoForm->setContentsMargins(4, 4, 4, 4);
    typoForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    typoForm->setRowWrapPolicy(QFormLayout::WrapLongRows);

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
    m_color->setFixedWidth(52);

    m_caseUpperBtn = new QToolButton(this);
    m_caseUpperBtn->setText(QStringLiteral("TT"));
    m_caseUpperBtn->setToolTip(QStringLiteral("Converter para MAIÚSCULAS"));
    m_caseTitleBtn = new QToolButton(this);
    m_caseTitleBtn->setText(QStringLiteral("Tt"));
    m_caseTitleBtn->setToolTip(QStringLiteral("Converter Para Formato Título"));
    m_caseLowerBtn = new QToolButton(this);
    m_caseLowerBtn->setText(QStringLiteral("tt"));
    m_caseLowerBtn->setToolTip(QStringLiteral("Converter para minúsculas"));

    m_styleLabel = new QLabel(this);
    auto* styleRow = new QHBoxLayout;
    styleRow->addWidget(m_bold);
    styleRow->addWidget(m_italic);
    styleRow->addWidget(m_underline);
    styleRow->addWidget(m_color);
    styleRow->addSpacing(4);
    styleRow->addWidget(m_caseUpperBtn);
    styleRow->addWidget(m_caseTitleBtn);
    styleRow->addWidget(m_caseLowerBtn);
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
    m_outlineColor->setFixedWidth(64);
    m_outlineWidth = new QDoubleSpinBox(this);
    m_outlineWidth->setRange(0.5, 40.0);
    m_outlineWidth->setSingleStep(0.5);
    m_outlineWidth->setSuffix(QStringLiteral(" px"));
    auto* outlineRow = new QHBoxLayout;
    outlineRow->addWidget(m_outlineOn);
    outlineRow->addWidget(m_outlineColor);
    outlineRow->addWidget(m_outlineWidth);
    outlineRow->addStretch();
    fxLayout->addLayout(outlineRow);

    m_shadowOn = new QCheckBox(this);
    m_shadowColor = new QPushButton(this);
    m_shadowColor->setFixedWidth(64);
    auto* shadowHeaderRow = new QHBoxLayout;
    shadowHeaderRow->addWidget(m_shadowOn);
    shadowHeaderRow->addWidget(m_shadowColor);
    shadowHeaderRow->addStretch();
    fxLayout->addLayout(shadowHeaderRow);

    m_shadowX = new QDoubleSpinBox(this);
    m_shadowX->setRange(-200.0, 200.0);
    m_shadowX->setPrefix(QStringLiteral("X: "));
    m_shadowY = new QDoubleSpinBox(this);
    m_shadowY->setRange(-200.0, 200.0);
    m_shadowY->setPrefix(QStringLiteral("Y: "));
    m_shadowBlur = new QDoubleSpinBox(this);
    m_shadowBlur->setRange(0.0, 60.0);
    m_shadowBlur->setSingleStep(1.0);
    m_shadowBlur->setPrefix(QStringLiteral("Blur: "));
    auto* shadowValsRow = new QHBoxLayout;
    shadowValsRow->addWidget(m_shadowX);
    shadowValsRow->addWidget(m_shadowY);
    shadowValsRow->addWidget(m_shadowBlur);
    fxLayout->addLayout(shadowValsRow);

    m_hollowOn = new QCheckBox(this);
    m_knockoutOn = new QCheckBox(this);
    fxLayout->addWidget(m_hollowOn);
    fxLayout->addWidget(m_knockoutOn);

    m_effectsSection->setContentLayout(fxLayout);
    layout->addWidget(m_effectsSection);

    // 2.1 Seção Fundo / Tarja (TextBadge)
    m_badgeSection = new CollapsibleSection(QStringLiteral("Fundo / Tarja"), false, this);
    auto* badgeLayout = new QVBoxLayout;
    badgeLayout->setContentsMargins(4, 4, 4, 4);
    badgeLayout->setSpacing(4);

    m_badgeOn = new QCheckBox(this);
    m_badgeColor = new QPushButton(this);
    auto* badgeRow1 = new QHBoxLayout;
    badgeRow1->addWidget(m_badgeOn);
    badgeRow1->addWidget(m_badgeColor);
    badgeRow1->addStretch();
    badgeLayout->addLayout(badgeRow1);

    auto* badgeForm = new QFormLayout;
    badgeForm->setContentsMargins(0, 0, 0, 0);

    m_badgePaddingX = new QDoubleSpinBox(this);
    m_badgePaddingX->setRange(0.0, 200.0);
    m_badgePaddingX->setSingleStep(2.0);
    m_badgePaddingX->setSuffix(QStringLiteral(" px"));

    m_badgePaddingY = new QDoubleSpinBox(this);
    m_badgePaddingY->setRange(0.0, 200.0);
    m_badgePaddingY->setSingleStep(2.0);
    m_badgePaddingY->setSuffix(QStringLiteral(" px"));

    m_badgeRadius = new QDoubleSpinBox(this);
    m_badgeRadius->setRange(0.0, 100.0);
    m_badgeRadius->setSingleStep(1.0);
    m_badgeRadius->setSuffix(QStringLiteral(" px"));

    badgeForm->addRow(new QLabel(QStringLiteral("Margem Horiz.:"), this), m_badgePaddingX);
    badgeForm->addRow(new QLabel(QStringLiteral("Margem Vert.:"), this), m_badgePaddingY);
    badgeForm->addRow(new QLabel(QStringLiteral("Arredondamento:"), this), m_badgeRadius);
    badgeLayout->addLayout(badgeForm);

    m_badgeSection->setContentLayout(badgeLayout);
    layout->addWidget(m_badgeSection);

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

    // --- Presets de estilo (modo avançado; escondido por omissão) ---
    m_styleSection = new CollapsibleSection(QStringLiteral("Estilos de texto"), false, this);
    auto* styleLayout = new QHBoxLayout;
    styleLayout->setContentsMargins(4, 4, 4, 4);
    m_styleCombo = new QComboBox(this);
    m_styleApplyBtn = new QPushButton(QStringLiteral("Aplicar"), this);
    m_styleSaveBtn = new QPushButton(QStringLiteral("Guardar estilo…"), this);
    m_styleRemoveBtn = new QPushButton(QStringLiteral("Remover"), this);
    connect(m_styleApplyBtn, &QPushButton::clicked, this, [this]() {
        if (!m_styleCombo || !m_styleStore)
            return;
        const QString name = m_styleCombo->currentData().toString();
        if (auto style = m_styleStore->find(name))
            applyTextStyle(*style);
    });
    connect(m_styleSaveBtn, &QPushButton::clicked, this, &TextInspector::saveCurrentAsStyle);
    connect(m_styleRemoveBtn, &QPushButton::clicked, this, [this]() {
        if (!m_styleCombo || !m_styleStore)
            return;
        const QString name = m_styleCombo->currentData().toString();
        if (m_styleStore->removeUserStyle(name))
            refreshStyleCombo();
    });
    styleLayout->addWidget(m_styleCombo, 1);
    styleLayout->addWidget(m_styleApplyBtn);
    styleLayout->addWidget(m_styleSaveBtn);
    styleLayout->addWidget(m_styleRemoveBtn);
    m_styleSection->setContentLayout(styleLayout);
    m_styleSection->setVisible(false);   // revelado pelo modo avançado
    layout->addWidget(m_styleSection);

    // --- Texto em caminho (modo avançado; escondido por omissão) ---
    // Nada de strings fixas: o projecto exige i18n em todos os catálogos.
    const QString pathNoneText = m_i18n ? m_i18n->t("editor", "textPath.none")
                                        : QStringLiteral("(sem caminho)");
    m_pathSection = new CollapsibleSection(
        m_i18n ? m_i18n->t("editor", "textPath.title") : QStringLiteral("Texto em Caminho"),
        false, this);
    auto* pathLayout = new QVBoxLayout;
    pathLayout->setContentsMargins(4, 4, 4, 4);
    m_pathCombo = new QComboBox(this);
    m_pathCombo->addItem(pathNoneText, QUuid().toString());
    pathLayout->addWidget(m_pathCombo);

    // Orientação: sem um caminho vectorial a combo fica com uma única
    // opção e o utilizador não percebe onde arranjar um. Dizemos quantos
    // existem e oferecemos um botão que passa para a caneta (P).
    m_pathHintLabel = new QLabel(this);
    m_pathHintLabel->setWordWrap(true);
    m_pathHintLabel->setStyleSheet(QStringLiteral("color: #9aa3b2; font-size: 11px;"));
    pathLayout->addWidget(m_pathHintLabel);

    m_createPathButton = new QPushButton(
        m_i18n ? m_i18n->t("editor", "textPath.createWithPen")
               : QStringLiteral("✏️ Desenhar caminho com a caneta (P)"),
        this);
    connect(m_createPathButton, &QPushButton::clicked, this, [this]() {
        // Pede à janela para activar a caneta (pelo mesmo caminho do
        // menu/atalho) em vez de mexer em ferramentas a partir daqui.
        emit requestTool(CanvasTool::Vector);
        if (m_pathHintLabel)
            m_pathHintLabel->setText(m_i18n
                ? m_i18n->t("editor", "textPath.hintDrawing")
                : QStringLiteral("Clique para criar os pontos do caminho; botão direito fecha. "
                                 "O texto passa a seguir o caminho assim que o ligar acima."));
    });
    pathLayout->addWidget(m_createPathButton);
    auto* offsetRow = new QHBoxLayout;
    offsetRow->addWidget(new QLabel(
        m_i18n ? m_i18n->t("editor", "textPath.offset") : QStringLiteral("Deslocamento:"),
        this));
    m_pathOffset = new QSlider(Qt::Horizontal, this);
    m_pathOffset->setRange(0, 100);
    m_pathOffset->setValue(0);
    offsetRow->addWidget(m_pathOffset);
    pathLayout->addLayout(offsetRow);
    m_pathSection->setContentLayout(pathLayout);
    m_pathSection->setVisible(false);
    layout->addWidget(m_pathSection);

    connect(m_pathCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &TextInspector::onPathLinkChanged);
    connect(m_pathOffset, &QSlider::valueChanged, this, &TextInspector::onPathOffsetChanged);

    layout->addStretch();
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setWidget(content);
    outerLayout->addWidget(scroll);

    for (auto* section : findChildren<CollapsibleSection*>()) {
        connect(section, &CollapsibleSection::toggled,
                this, [this] { updateGeometry(); });
    }

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

    auto convertCase = [this](int mode) {
        TextLayer* layer = nullptr;
        if (!editingLayer(&layer) || !m_document) return;
        const QString oldText = layer->content;
        QString newText;
        if (mode == 0) newText = oldText.toUpper();
        else if (mode == 1) newText = toTitleCase(oldText);
        else if (mode == 2) newText = oldText.toLower();

        if (newText == oldText) return;
        m_content->setPlainText(newText);
        if (m_history) {
            m_history->execute(std::make_unique<LayerPropertyCommand<QString>>(
                *m_document, m_id, QStringLiteral("layer.text"),
                &Document::setLayerTextContent, oldText, newText));
        } else {
            m_document->setLayerTextContent(m_id, newText);
        }
        touch();
    };
    connect(m_caseUpperBtn, &QToolButton::clicked, this, [convertCase] { convertCase(0); });
    connect(m_caseTitleBtn, &QToolButton::clicked, this, [convertCase] { convertCase(1); });
    connect(m_caseLowerBtn, &QToolButton::clicked, this, [convertCase] { convertCase(2); });

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

    connect(m_badgeOn, &QCheckBox::toggled, this, [applyEffects](bool on) {
        applyEffects([on](TextEffects& fx) { fx.badge.enabled = on; });
    });
    connect(m_badgeColor, &QPushButton::clicked, this, [this, applyEffects] {
        TextLayer* layer = nullptr;
        if (!editingLayer(&layer)) return;
        const QColor chosen = QColorDialog::getColor(layer->effects.badge.color, this);
        if (!chosen.isValid()) return;
        const QString hex = chosen.name(QColor::HexRgb);
        m_badgeColor->setText(hex);
        m_badgeColor->setStyleSheet(QStringLiteral("background: %1;").arg(hex));
        applyEffects([chosen](TextEffects& fx) { fx.badge.color = chosen; });
    });
    connect(m_badgePaddingX, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [applyEffects](double value) {
        applyEffects([value](TextEffects& fx) { fx.badge.paddingX = value; });
    });
    connect(m_badgePaddingY, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [applyEffects](double value) {
        applyEffects([value](TextEffects& fx) { fx.badge.paddingY = value; });
    });
    connect(m_badgeRadius, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [applyEffects](double value) {
        applyEffects([value](TextEffects& fx) { fx.badge.cornerRadius = value; });
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

    if (m_quickPresetsSection)
        m_quickPresetsSection->setTitle(m_i18n ? m_i18n->t("editor", "section.quickPresets") : QStringLiteral("Presets Rápidos (Thumbnails)"));
    if (m_typographySection)
        m_typographySection->setTitle(m_i18n->t("editor", "section.typography"));
    if (m_effectsSection)
        m_effectsSection->setTitle(m_i18n->t("editor", "section.textEffects"));
    if (m_badgeSection)
        m_badgeSection->setTitle(m_i18n ? m_i18n->t("editor", "section.badge") : QStringLiteral("Fundo / Tarja"));
    if (m_gradientSection)
        m_gradientSection->setTitle(m_i18n->t("editor", "section.gradient"));
    if (m_pathSection)
        m_pathSection->setTitle(m_i18n->t("editor", "textPath.title"));
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
    if (m_badgeOn)
        m_badgeOn->setText(m_i18n ? m_i18n->t("editor", "effects.badge") : QStringLiteral("Tarja de Fundo"));
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


void TextInspector::setStyleStore(cc::TextStyleStore* store)
{
    m_styleStore = store;
    refreshStyleCombo();
}

void TextInspector::refreshStyleCombo()
{
    if (!m_styleCombo)
        return;
    const QString current = m_styleCombo->currentText();
    m_styleCombo->clear();
    if (m_styleStore) {
        for (const auto& style : m_styleStore->all()) {
            const bool user = m_styleStore->isUserStyle(style.name);
            m_styleCombo->addItem(user ? style.name + QStringLiteral(" *") : style.name,
                                  style.name);
        }
    }
    if (!current.isEmpty()) {
        const int index = m_styleCombo->findData(current);
        if (index >= 0)
            m_styleCombo->setCurrentIndex(index);
    }
}


void TextInspector::applyTextStyle(const cc::TextStyle& style)
{
    TextLayer* layer = nullptr;
    if (!editingLayer(&layer) || !m_history)
        return;

    // Cada propriedade é um comando próprio (undo granular); o efeito é um
    // bloco único porque o inspector já o trata assim.
    m_history->execute(std::make_unique<SetTextFontFamilyCommand>(
        *m_document, m_id, QStringLiteral("text.fontFamily"),
        &Document::setLayerFontFamily, layer->fontFamily, style.fontFamily));
    m_history->execute(std::make_unique<SetTextSizeCommand>(
        *m_document, m_id, QStringLiteral("text.size"),
        &Document::setLayerTextSize, layer->sizePt, style.sizePt));
    m_history->execute(std::make_unique<SetTextColorCommand>(
        *m_document, m_id, QStringLiteral("text.color"),
        &Document::setLayerTextColor, layer->color, style.color));
    m_history->execute(std::make_unique<SetTextBoldCommand>(
        *m_document, m_id, QStringLiteral("text.bold"),
        &Document::setLayerTextBold, layer->bold, style.bold));
    m_history->execute(std::make_unique<SetTextItalicCommand>(
        *m_document, m_id, QStringLiteral("text.italic"),
        &Document::setLayerTextItalic, layer->italic, style.italic));
    m_history->execute(std::make_unique<SetTextLetterSpacingCommand>(
        *m_document, m_id, QStringLiteral("text.letterSpacing"),
        &Document::setLayerLetterSpacing, layer->letterSpacingPx, style.letterSpacingPx));
    m_history->execute(std::make_unique<SetTextLineHeightCommand>(
        *m_document, m_id, QStringLiteral("text.lineHeight"),
        &Document::setLayerLineHeight, layer->lineHeightMult, style.lineHeightMult));
    m_history->execute(std::make_unique<SetLayerTextEffectsCommand>(
        *m_document, m_id, layer->effects, style.effects));

    refresh();
}

void TextInspector::saveCurrentAsStyle()
{
    TextLayer* layer = nullptr;
    if (!editingLayer(&layer) || !m_styleStore)
        return;

    bool ok = false;
    const QString name = QInputDialog::getText(
        this, QStringLiteral("Guardar estilo"),
        QStringLiteral("Nome do estilo:"), QLineEdit::Normal,
        m_styleCombo ? m_styleCombo->currentData().toString() : QString(), &ok);
    if (!ok || name.trimmed().isEmpty())
        return;

    cc::TextStyle style;
    style.name = name.trimmed();
    style.fontFamily = layer->fontFamily;
    style.sizePt = layer->sizePt;
    style.bold = layer->bold;
    style.italic = layer->italic;
    style.color = layer->color;
    style.letterSpacingPx = layer->letterSpacingPx;
    style.lineHeightMult = layer->lineHeightMult;
    style.effects = layer->effects;

    if (m_styleStore->addUserStyle(style))
        refreshStyleCombo();
    else
        QMessageBox::warning(this, QStringLiteral("Guardar estilo"),
                             QStringLiteral("Não foi possível guardar: nome vazio, duplicado ou erro de escrita."));
}


void TextInspector::refreshPathCombo()
{
    if (!m_pathCombo)
        return;
    const QString current = m_pathCombo->currentData().toString();
    m_pathCombo->clear();
    // i18n: era string fixa em português.
    m_pathCombo->addItem(m_i18n ? m_i18n->t("editor", "textPath.none")
                                : QStringLiteral("(sem caminho)"),
                         QUuid().toString());
    int pathCount = 0;
    if (m_document) {
        // Percorrer a árvore e listar as camadas vectoriais.
        std::vector<const GroupLayer*> stack;
        if (const GroupLayer* root = m_document->rootGroup())
            stack.push_back(root);
        while (!stack.empty()) {
            const GroupLayer* group = stack.back();
            stack.pop_back();
            for (const auto& child : group->children) {
                if (!child)
                    continue;
                if (child->type() == LayerType::Vector) {
                    const QString name = child->name.isEmpty()
                        ? (m_i18n ? m_i18n->t("editor", "textPath.defaultPathName")
                                  : QStringLiteral("Caminho"))
                        : child->name;
                    m_pathCombo->addItem(name, child->id().toString(QUuid::WithoutBraces));
                    ++pathCount;
                }
                if (child->type() == LayerType::Group)
                    stack.push_back(static_cast<const GroupLayer*>(child.get()));
            }
        }
    }
    if (!current.isEmpty()) {
        const int index = m_pathCombo->findData(current);
        if (index >= 0)
            m_pathCombo->setCurrentIndex(index);
    }

    // Orientação contextual: quantos caminhos existem e o que fazer se não
    // houver nenhum — era o que deixava o utilizador perdido.
    if (m_pathHintLabel) {
        if (pathCount == 0) {
            m_pathHintLabel->setText(m_i18n
                ? m_i18n->t("editor", "textPath.hintNone")
                : QStringLiteral("Ainda não existe nenhum caminho. Use a caneta (P) para "
                                 "desenhar um e ele aparece aqui."));
        } else {
            m_pathHintLabel->setText(m_i18n
                ? m_i18n->t("editor", "textPath.hintCount", {QString::number(pathCount)})
                : QStringLiteral("%1 caminho(s) disponível(is) — escolha um acima.")
                      .arg(pathCount));
        }
    }
}

void TextInspector::onPathLinkChanged(int index)
{
    TextLayer* layer = nullptr;
    if (index < 0 || !editingLayer(&layer) || !m_history || !m_pathCombo)
        return;

    const QString pathData = m_pathCombo->itemData(index).toString();
    std::optional<LayerId> newPath;
    if (!pathData.isEmpty()) {
        const QUuid parsed(pathData);
        if (!parsed.isNull())
            newPath = parsed;
    }
    m_history->execute(std::make_unique<cc::SetTextPathLayerCommand>(
        *m_document, m_id, layer->pathLayerId, newPath));
    refresh();
}

void TextInspector::onPathOffsetChanged(int value)
{
    TextLayer* layer = nullptr;
    if (!editingLayer(&layer) || !m_history)
        return;
    const double offset = std::clamp(value / 100.0, 0.0, 1.0);
    m_history->execute(std::make_unique<cc::SetTextPathOffsetCommand>(
        *m_document, m_id, layer->pathOffsetPct, offset));
    refresh();
}

void TextInspector::setAdvancedMode(bool advanced)
{
    if (m_styleSection)
        m_styleSection->setVisible(advanced);
    if (m_pathSection)
        m_pathSection->setVisible(advanced);
}

} // namespace cc
