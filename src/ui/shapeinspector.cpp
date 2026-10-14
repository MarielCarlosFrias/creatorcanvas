#include "shapeinspector.h"

// Inclusão dos serviços de internacionalização e histórico de comandos
#include "localization/i18nservice.h"
#include "core/history/CommandStack.h"
#include "core/history/DocumentCommands.h"

// Inclusões de componentes de interface gráfica do Qt
#include <QCheckBox>
#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QFrame>

namespace cc {

ShapeInspector::ShapeInspector(I18nService* i18n, Document* document,
                               CommandStack* history, QWidget* parent)
    : QWidget(parent)
    , m_i18n(i18n)
    , m_document(document)
    , m_history(history)
{
    // Constrói toda a estrutura visual de controles e layouts
    buildUi();

    // Conecta a mudança dinâmica de idioma para atualizar os textos em tempo real
    if (m_i18n) {
        connect(m_i18n, &I18nService::languageChanged,
                this, &ShapeInspector::retranslateUi);
    }
    retranslateUi();

    // Começa oculto até que uma ShapeLayer válida seja selecionada
    setVisible(false);
}

// Verifica se a camada selecionada no momento é de fato uma ShapeLayer
bool ShapeInspector::editingLayer(ShapeLayer** out) const
{
    if (!m_document || m_id.isNull())
        return false;

    Layer* layer = m_document->findLayer(m_id);
    if (!layer || layer->type() != LayerType::Shape)
        return false;

    if (out)
        *out = static_cast<ShapeLayer*>(layer);
    return true;
}

// Atualiza a referência do documento ativo (ao abrir ou criar novo projeto)
void ShapeInspector::setDocument(Document* document)
{
    m_document = document;
    m_id = LayerId();
    refresh();
}

// Atualiza o ID da camada atualmente selecionada pelo usuário
void ShapeInspector::setSelectedLayer(const LayerId& id)
{
    m_id = id;
    refresh();
}

// Atualiza o botão de cor com um preview visual da cor e contraste do texto
void ShapeInspector::updateColorButton(QPushButton* button, const QColor& color)
{
    if (!button)
        return;

    // Se a cor for totalmente transparente, desenha uma borda tracejada indicando "Sem cor"
    if (!color.isValid() || color.alpha() == 0) {
        button->setText(QStringLiteral("None"));
        button->setStyleSheet(QStringLiteral(
            "QPushButton { background: transparent; color: #888; "
            "border: 1px dashed #666; border-radius: 4px; padding: 4px; }"));
        return;
    }

    const QString hex = color.name(QColor::HexRgb);

    // Calcula a luminosidade percebida (fórmula ITU-R BT.601) para garantir legibilidade do texto do botão
    const int luminance = (color.red() * 299 + color.green() * 587 + color.blue() * 114) / 1000;
    const QString textColor = luminance > 128 ? QStringLiteral("#000000") : QStringLiteral("#ffffff");

    button->setText(hex);
    button->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: %1; color: %2; font-weight: bold; "
        "border: 1px solid #444; border-radius: 4px; padding: 4px; }")
        .arg(hex, textColor));
}

// Sincroniza todos os widgets com os dados da camada selecionada
void ShapeInspector::refresh()
{
    ShapeLayer* layer = nullptr;
    const bool isShape = editingLayer(&layer);

    // Oculta o inspetor se a seleção atual não for uma forma geométrica
    setVisible(isShape);
    if (!isShape || !layer)
        return;

    // Flag m_loading previne que os sinais dos widgets disparem mutações durante a carga inicial
    m_loading = true;

    // Atualiza o tipo de forma selecionado no combo
    for (int i = 0; i < m_kindCombo->count(); ++i) {
        if (m_kindCombo->itemData(i).toInt() == static_cast<int>(layer->kind)) {
            m_kindCombo->setCurrentIndex(i);
            break;
        }
    }

    // Atualiza os botões de cores
    updateColorButton(m_fillColorBtn, layer->fill);
    updateColorButton(m_strokeColorBtn, layer->stroke);

    // Atualiza os spinboxes
    m_strokeWidthSpin->setValue(layer->strokeWidth);
    m_cornerRadiusSpin->setValue(layer->cornerRadius);

    // Para linhas retas, não há preenchimento; desabilita o botão para melhor UX
    const bool canHaveFill = (layer->kind != ShapeKind::Line);
    m_fillColorBtn->setEnabled(canHaveFill);
    m_fillLabel->setEnabled(canHaveFill);

    // Estrela / Tracejado
    m_starPointsSpin->setValue(layer->starPoints);
    m_starRatioSpin->setValue(layer->starInnerRatio);
    m_dashRatioSpin->setValue(layer->strokeDashRatio);
    const bool isStar = (layer->kind == ShapeKind::Star);
    m_starPointsLabel->setVisible(isStar);
    m_starPointsSpin->setVisible(isStar);
    m_starRatioLabel->setVisible(isStar);
    m_starRatioSpin->setVisible(isStar);
    m_dashLabel->setVisible(layer->strokeWidth > 0.0);
    m_dashRatioSpin->setVisible(layer->strokeWidth > 0.0);

    // Sombra
    m_shadowOn->setChecked(layer->shadow.enabled);
    updateColorButton(m_shadowColorBtn,
                      layer->shadow.color.isValid()
                          ? layer->shadow.color : QColor(0, 0, 0, 170));
    m_shadowXSpin->setValue(layer->shadow.offsetX);
    m_shadowYSpin->setValue(layer->shadow.offsetY);
    m_shadowBlurSpin->setValue(layer->shadow.blur);
    const bool shadowOn = layer->shadow.enabled;
    m_shadowColorLabel->setEnabled(shadowOn);
    m_shadowColorBtn->setEnabled(shadowOn);
    m_shadowXLabel->setEnabled(shadowOn);
    m_shadowXSpin->setEnabled(shadowOn);
    m_shadowYLabel->setEnabled(shadowOn);
    m_shadowYSpin->setEnabled(shadowOn);
    m_shadowBlurLabel->setEnabled(shadowOn);
    m_shadowBlurSpin->setEnabled(shadowOn);

    // Gradiente
    m_gradientOn->setChecked(layer->gradient.enabled);
    m_gradientKindCombo->setCurrentIndex(
        layer->gradient.kind == GradientKind::Radial ? 1 : 0);
    m_gradientAngleSpin->setValue(layer->gradient.angleDeg);
    const bool hasStops = layer->gradient.stops.size() >= 2;
    updateColorButton(m_gradientStartBtn,
                      hasStops ? layer->gradient.stops.first().color : QColor(255, 210, 0));
    updateColorButton(m_gradientEndBtn,
                      hasStops ? layer->gradient.stops.last().color : QColor(255, 45, 0));
    const bool gradEnabled = layer->gradient.enabled;
    m_gradientKindCombo->setEnabled(gradEnabled);
    m_gradientAngleSpin->setEnabled(gradEnabled && layer->gradient.kind == GradientKind::Linear);
    m_gradientStartBtn->setEnabled(gradEnabled);
    m_gradientEndBtn->setEnabled(gradEnabled);
    m_gradientKindLabel->setEnabled(gradEnabled);
    m_gradientAngleLabel->setEnabled(gradEnabled && layer->gradient.kind == GradientKind::Linear);
    m_gradientStartLabel->setEnabled(gradEnabled);
    m_gradientEndLabel->setEnabled(gradEnabled);

    // O raio dos cantos aplica-se a retângulos arredondados e balões de fala
    const bool canHaveRadius = (layer->kind == ShapeKind::RoundedRect || layer->kind == ShapeKind::SpeechBubble);
    m_cornerRadiusLabel->setVisible(canHaveRadius);
    m_cornerRadiusSpin->setVisible(canHaveRadius);

    m_loading = false;
}

// Constrói os componentes da interface gráfica do inspetor
void ShapeInspector::buildUi()
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    auto* content = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(content);
    mainLayout->setContentsMargins(4, 4, 4, 4);

    m_groupBox = new QGroupBox(content);
    m_formLayout = new QFormLayout(m_groupBox);
    m_formLayout->setRowWrapPolicy(QFormLayout::WrapLongRows);

    // 0. Seletor de Tipo de Forma (Shape Kind)
    m_kindLabel = new QLabel(m_groupBox);
    m_kindCombo = new QComboBox(m_groupBox);
    m_formLayout->addRow(m_kindLabel, m_kindCombo);

    connect(m_kindCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (m_loading || idx < 0)
            return;
        ShapeLayer* layer = nullptr;
        if (!editingLayer(&layer))
            return;

        const auto newKind = static_cast<ShapeKind>(m_kindCombo->itemData(idx).toInt());
        if (layer->kind == newKind)
            return;

        if (m_history && m_document) {
            m_history->execute(std::make_unique<SetShapeKindCommand>(
                *m_document, m_id, QStringLiteral("shape.kind"),
                &Document::setShapeKind, layer->kind, newKind));
        } else if (m_document) {
            m_document->setShapeKind(m_id, newKind);
        }

        const bool canHaveFill = (newKind != ShapeKind::Line);
        m_fillColorBtn->setEnabled(canHaveFill);
        m_fillLabel->setEnabled(canHaveFill);

        const bool canHaveRadius = (newKind == ShapeKind::RoundedRect || newKind == ShapeKind::SpeechBubble);
        m_cornerRadiusLabel->setVisible(canHaveRadius);
        m_cornerRadiusSpin->setVisible(canHaveRadius);
    });

    // 1. Botão e label de Cor de Preenchimento (Fill Color)
    m_fillLabel = new QLabel(m_groupBox);
    m_fillColorBtn = new QPushButton(m_groupBox);
    m_formLayout->addRow(m_fillLabel, m_fillColorBtn);

    // Ao clicar, abre o diálogo de cores nativo do Qt (multiplataforma Linux/Windows)
    connect(m_fillColorBtn, &QPushButton::clicked, this, [this] {
        ShapeLayer* layer = nullptr;
        if (!editingLayer(&layer))
            return;

        const QColor chosen = QColorDialog::getColor(
            layer->fill.isValid() ? layer->fill : Qt::white,
            this, QString(), QColorDialog::ShowAlphaChannel);

        if (!chosen.isValid())
            return;

        // Se houver CommandStack, executa via comando com suporte completo a Undo/Redo
        if (m_history && m_document) {
            m_history->execute(std::make_unique<SetShapeFillCommand>(
                *m_document, m_id, QStringLiteral("shape.fill"),
                &Document::setShapeFill, layer->fill, chosen));
        } else if (m_document) {
            m_document->setShapeFill(m_id, chosen);
        }

        updateColorButton(m_fillColorBtn, chosen);
    });

    // 1a-bis. Parametros de Estrela e Traco Tracejado
    m_starPointsLabel = new QLabel(m_groupBox);
    m_starPointsSpin = new QSpinBox(m_groupBox);
    m_starPointsSpin->setRange(3, 16);
    m_formLayout->addRow(m_starPointsLabel, m_starPointsSpin);

    m_starRatioLabel = new QLabel(m_groupBox);
    m_starRatioSpin = new QDoubleSpinBox(m_groupBox);
    m_starRatioSpin->setRange(0.1, 0.9);
    m_starRatioSpin->setSingleStep(0.05);
    m_starRatioSpin->setDecimals(2);
    m_formLayout->addRow(m_starRatioLabel, m_starRatioSpin);

    m_dashLabel = new QLabel(m_groupBox);
    m_dashRatioSpin = new QDoubleSpinBox(m_groupBox);
    m_dashRatioSpin->setRange(0.0, 10.0);
    m_dashRatioSpin->setSingleStep(0.5);
    m_dashRatioSpin->setDecimals(1);
    m_dashRatioSpin->setSpecialValueText(QStringLiteral("Solid"));
    m_formLayout->addRow(m_dashLabel, m_dashRatioSpin);

    auto commitShapeField = [this](auto commitOne) {
        if (m_loading) return;
        ShapeLayer* layer = nullptr;
        if (editingLayer(&layer)) commitOne(layer);
    };
    connect(m_starPointsSpin, qOverload<int>(&QSpinBox::valueChanged), this,
            [this, commitShapeField](int v) {
        commitShapeField([this, v](ShapeLayer* layer) {
            if (m_history && m_document)
                m_history->execute(std::make_unique<SetShapeStarPointsCommand>(
                    *m_document, m_id, QStringLiteral("shape.starPoints"),
                    &Document::setShapeStarPoints, layer->starPoints, v));
            else if (m_document)
                m_document->setShapeStarPoints(m_id, v);
        });
    });
    connect(m_starRatioSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this, commitShapeField](double v) {
        commitShapeField([this, v](ShapeLayer* layer) {
            if (m_history && m_document)
                m_history->execute(std::make_unique<SetShapeStarInnerRatioCommand>(
                    *m_document, m_id, QStringLiteral("shape.starInnerRatio"),
                    &Document::setShapeStarInnerRatio, layer->starInnerRatio, v));
            else if (m_document)
                m_document->setShapeStarInnerRatio(m_id, v);
        });
    });
    connect(m_dashRatioSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this, commitShapeField](double v) {
        commitShapeField([this, v](ShapeLayer* layer) {
            if (m_history && m_document)
                m_history->execute(std::make_unique<SetShapeStrokeDashRatioCommand>(
                    *m_document, m_id, QStringLiteral("shape.strokeDash"),
                    &Document::setShapeStrokeDashRatio, layer->strokeDashRatio, v));
            else if (m_document)
                m_document->setShapeStrokeDashRatio(m_id, v);
        });
    });

    // 1b. Secao de Gradiente (preenchimento com 2 cores)
    m_gradientBox = new QGroupBox(content);
    m_gradientBox->setObjectName(QStringLiteral("GradientSection"));
    m_gradientForm = new QFormLayout(m_gradientBox);
    m_gradientForm->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_gradientOn = new QCheckBox(m_gradientBox);
    m_gradientForm->addRow(m_gradientOn);
    m_gradientKindLabel = new QLabel(m_gradientBox);
    m_gradientKindCombo = new QComboBox(m_gradientBox);
    m_gradientForm->addRow(m_gradientKindLabel, m_gradientKindCombo);

    m_gradientAngleLabel = new QLabel(m_gradientBox);
    m_gradientAngleSpin = new QDoubleSpinBox(m_gradientBox);
    m_gradientAngleSpin->setRange(-360.0, 360.0);
    m_gradientAngleSpin->setDecimals(0);
    m_gradientAngleSpin->setValue(90.0);
    m_gradientForm->addRow(m_gradientAngleLabel, m_gradientAngleSpin);

    m_gradientStartLabel = new QLabel(m_gradientBox);
    m_gradientStartBtn = new QPushButton(m_gradientBox);
    m_gradientForm->addRow(m_gradientStartLabel, m_gradientStartBtn);

    m_gradientEndLabel = new QLabel(m_gradientBox);
    m_gradientEndBtn = new QPushButton(m_gradientBox);
    m_gradientForm->addRow(m_gradientEndLabel, m_gradientEndBtn);

    auto readGradientFromUi = [this]() -> Gradient {
        ShapeLayer* layer = nullptr;
        Gradient g;
        if (!editingLayer(&layer))
            return g;
        g = layer->gradient;
        g.enabled = m_gradientOn->isChecked();
        g.kind = m_gradientKindCombo->currentIndex() == 1
                     ? GradientKind::Radial : GradientKind::Linear;
        g.angleDeg = m_gradientAngleSpin->value();
        if (g.stops.size() < 2) {
            g.stops = {{0.0f, QColor(255, 210, 0)}, {1.0f, QColor(255, 45, 0)}};
        }
        return g;
    };

    connect(m_gradientOn, &QCheckBox::toggled, this, [this, readGradientFromUi](bool) {
        if (m_loading) return;
        commitGradient(readGradientFromUi());
    });
    connect(m_gradientKindCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this, readGradientFromUi](int) {
        if (m_loading) return;
        commitGradient(readGradientFromUi());
    });
    connect(m_gradientAngleSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this, readGradientFromUi](double) {
        if (m_loading) return;
        commitGradient(readGradientFromUi());
    });
    connect(m_gradientStartBtn, &QPushButton::clicked, this, [this, readGradientFromUi] {
        ShapeLayer* layer = nullptr;
        if (!editingLayer(&layer)) return;
        const QColor chosen = QColorDialog::getColor(
            layer->gradient.stops.isEmpty() ? QColor(255, 210, 0)
                                           : layer->gradient.stops.first().color,
            this, QString(), QColorDialog::ShowAlphaChannel);
        if (!chosen.isValid()) return;
        Gradient g = readGradientFromUi();
        if (g.stops.isEmpty())
            g.stops.append({0.0f, chosen});
        else
            g.stops.first().color = chosen;
        if (g.stops.size() < 2)
            g.stops.append({1.0f, QColor(255, 45, 0)});
        commitGradient(std::move(g));
    });
    connect(m_gradientEndBtn, &QPushButton::clicked, this, [this, readGradientFromUi] {
        ShapeLayer* layer = nullptr;
        if (!editingLayer(&layer)) return;
        const QColor chosen = QColorDialog::getColor(
            layer->gradient.stops.size() < 2 ? QColor(255, 45, 0)
                                             : layer->gradient.stops.last().color,
            this, QString(), QColorDialog::ShowAlphaChannel);
        if (!chosen.isValid()) return;
        Gradient g = readGradientFromUi();
        while (g.stops.size() < 2)
            g.stops.append({static_cast<float>(g.stops.size()), QColor(255, 210, 0)});
        g.stops.last().color = chosen;
        commitGradient(std::move(g));
    });

    // 1c. Secao Sombra Projetada
    m_shadowBox = new QGroupBox(content);
    m_shadowForm = new QFormLayout(m_shadowBox);
    m_shadowForm->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_shadowOn = new QCheckBox(m_shadowBox);
    m_shadowForm->addRow(m_shadowOn);

    m_shadowColorLabel = new QLabel(m_shadowBox);
    m_shadowColorBtn = new QPushButton(m_shadowBox);
    m_shadowForm->addRow(m_shadowColorLabel, m_shadowColorBtn);

    m_shadowXLabel = new QLabel(m_shadowBox);
    m_shadowXSpin = new QDoubleSpinBox(m_shadowBox);
    m_shadowXSpin->setRange(-200.0, 200.0);
    m_shadowXSpin->setDecimals(0);
    m_shadowForm->addRow(m_shadowXLabel, m_shadowXSpin);

    m_shadowYLabel = new QLabel(m_shadowBox);
    m_shadowYSpin = new QDoubleSpinBox(m_shadowBox);
    m_shadowYSpin->setRange(-200.0, 200.0);
    m_shadowYSpin->setDecimals(0);
    m_shadowForm->addRow(m_shadowYLabel, m_shadowYSpin);

    m_shadowBlurLabel = new QLabel(m_shadowBox);
    m_shadowBlurSpin = new QDoubleSpinBox(m_shadowBox);
    m_shadowBlurSpin->setRange(0.0, 60.0);
    m_shadowBlurSpin->setDecimals(0);
    m_shadowForm->addRow(m_shadowBlurLabel, m_shadowBlurSpin);

    auto readShadowFromUi = [this]() -> ShapeShadow {
        ShapeLayer* layer = nullptr;
        ShapeShadow s;
        if (!editingLayer(&layer))
            return s;
        s = layer->shadow;
        s.enabled = m_shadowOn->isChecked();
        s.offsetX = m_shadowXSpin->value();
        s.offsetY = m_shadowYSpin->value();
        s.blur = m_shadowBlurSpin->value();
        if (!s.color.isValid())
            s.color = QColor(0, 0, 0, 170);
        return s;
    };
    auto commitShadow = [this, readShadowFromUi]() {
        if (m_loading) return;
        ShapeLayer* layer = nullptr;
        if (!editingLayer(&layer)) return;
        ShapeShadow next = readShadowFromUi();
        if (layer->shadow == next) return;
        if (m_history && m_document)
            m_history->execute(std::make_unique<SetShapeShadowCommand>(
                *m_document, m_id, QStringLiteral("shape.shadow"),
                &Document::setShapeShadow, layer->shadow, std::move(next)));
        else if (m_document)
            m_document->setShapeShadow(m_id, std::move(next));
        // Re-sincroniza: sem isto os campos X/Y/cor ficavam desativados
        // depois de ligar a sombra (nunca havia um refresh).
        refresh();
    };
    connect(m_shadowOn, &QCheckBox::toggled, this, commitShadow);
    connect(m_shadowXSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, commitShadow);
    connect(m_shadowYSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, commitShadow);
    connect(m_shadowBlurSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, commitShadow);
    connect(m_shadowColorBtn, &QPushButton::clicked, this, [this, commitShadow, readShadowFromUi] {
        ShapeLayer* layer = nullptr;
        if (!editingLayer(&layer)) return;
        const QColor chosen = QColorDialog::getColor(
            layer->shadow.color.isValid() ? layer->shadow.color : QColor(0, 0, 0, 170),
            this, QString(), QColorDialog::ShowAlphaChannel);
        if (!chosen.isValid()) return;
        ShapeShadow s = readShadowFromUi();
        s.color = chosen;
        if (m_history && m_document)
            m_history->execute(std::make_unique<SetShapeShadowCommand>(
                *m_document, m_id, QStringLiteral("shape.shadow"),
                &Document::setShapeShadow, layer->shadow, s));
        else if (m_document)
            m_document->setShapeShadow(m_id, s);
        refresh();
    });

    // 2. Botão e label de Cor do Contorno (Stroke Color)
    m_strokeLabel = new QLabel(m_groupBox);
    m_strokeColorBtn = new QPushButton(m_groupBox);
    m_formLayout->addRow(m_strokeLabel, m_strokeColorBtn);

    connect(m_strokeColorBtn, &QPushButton::clicked, this, [this] {
        ShapeLayer* layer = nullptr;
        if (!editingLayer(&layer))
            return;

        const QColor chosen = QColorDialog::getColor(
            layer->stroke.isValid() ? layer->stroke : Qt::black,
            this, QString(), QColorDialog::ShowAlphaChannel);

        if (!chosen.isValid())
            return;

        if (m_history && m_document) {
            m_history->execute(std::make_unique<SetShapeStrokeCommand>(
                *m_document, m_id, QStringLiteral("shape.stroke"),
                &Document::setShapeStroke, layer->stroke, chosen));
        } else if (m_document) {
            m_document->setShapeStroke(m_id, chosen);
        }

        updateColorButton(m_strokeColorBtn, chosen);
    });

    // 3. Spinbox de Espessura do Contorno (Stroke Width)
    m_strokeWidthLabel = new QLabel(m_groupBox);
    m_strokeWidthSpin = new QDoubleSpinBox(m_groupBox);
    m_strokeWidthSpin->setRange(0.0, 100.0);
    m_strokeWidthSpin->setSingleStep(1.0);
    m_strokeWidthSpin->setDecimals(1);
    m_strokeWidthSpin->setSuffix(QStringLiteral(" px"));
    m_formLayout->addRow(m_strokeWidthLabel, m_strokeWidthSpin);

    // Mudança ao vivo enquanto o usuário altera o valor no spinbox
    connect(m_strokeWidthSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this](double value) {
        if (m_loading || !m_document)
            return;
        m_document->setShapeStrokeWidth(m_id, value);
    });

    // 4. Spinbox de Raio dos Cantos (Corner Radius para RoundedRect)
    m_cornerRadiusLabel = new QLabel(m_groupBox);
    m_cornerRadiusSpin = new QDoubleSpinBox(m_groupBox);
    m_cornerRadiusSpin->setRange(0.0, 500.0);
    m_cornerRadiusSpin->setSingleStep(1.0);
    m_cornerRadiusSpin->setDecimals(1);
    m_cornerRadiusSpin->setSuffix(QStringLiteral(" px"));
    m_formLayout->addRow(m_cornerRadiusLabel, m_cornerRadiusSpin);

    // Atualiza o raio ao vivo
    connect(m_cornerRadiusSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this](double value) {
        if (m_loading || !m_document)
            return;
        m_document->setShapeCornerRadius(m_id, value);
    });

    mainLayout->addWidget(m_groupBox);
    mainLayout->addWidget(m_gradientBox);
    mainLayout->addWidget(m_shadowBox);
    mainLayout->addStretch();
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setWidget(content);
    outerLayout->addWidget(scroll);
}

// Atualiza todos os textos com base no serviço de internacionalização
void ShapeInspector::commitGradient(Gradient next)
{
    ShapeLayer* layer = nullptr;
    if (!editingLayer(&layer))
        return;
    if (layer->gradient == next)
        return;
    if (m_history && m_document) {
        m_history->execute(std::make_unique<SetShapeGradientCommand>(
            *m_document, m_id, QStringLiteral("shape.gradient"),
            &Document::setShapeGradient, layer->gradient, std::move(next)));
    } else if (m_document) {
        m_document->setShapeGradient(m_id, std::move(next));
    }
    refresh();
}

void ShapeInspector::retranslateUi()
{
    if (!m_i18n)
        return;

    m_groupBox->setTitle(m_i18n->t("editor", "shape.title"));
    m_kindLabel->setText(m_i18n->t("editor", "shape.type"));
    m_fillLabel->setText(m_i18n->t("editor", "shape.fill"));
    m_strokeLabel->setText(m_i18n->t("editor", "shape.stroke"));
    m_strokeWidthLabel->setText(m_i18n->t("editor", "shape.strokeWidth"));
    m_cornerRadiusLabel->setText(m_i18n->t("editor", "shape.cornerRadius"));
    m_gradientOn->setText(m_i18n->t("editor", "shape.gradient.enable"));
    m_gradientKindLabel->setText(m_i18n->t("editor", "shape.gradient.kind"));
    m_gradientAngleLabel->setText(m_i18n->t("editor", "shape.gradient.angle"));
    m_gradientStartLabel->setText(m_i18n->t("editor", "shape.gradient.start"));
    m_gradientEndLabel->setText(m_i18n->t("editor", "shape.gradient.end"));
    m_gradientBox->setTitle(m_i18n->t("editor", "shape.gradient.title"));
    const int gk = m_gradientKindCombo->currentIndex();
    m_gradientKindCombo->clear();
    m_gradientKindCombo->addItem(m_i18n->t("editor", "shape.gradient.linear"));
    m_gradientKindCombo->addItem(m_i18n->t("editor", "shape.gradient.radial"));
    m_gradientKindCombo->setCurrentIndex(gk >= 0 ? gk : 0);
    m_starPointsLabel->setText(m_i18n->t("editor", "shape.starPoints"));
    m_starRatioLabel->setText(m_i18n->t("editor", "shape.starSharpness"));
    m_dashLabel->setText(m_i18n->t("editor", "shape.strokeDash"));
    m_shadowOn->setText(m_i18n->t("editor", "shape.shadow.enable"));
    m_shadowColorLabel->setText(m_i18n->t("editor", "shape.shadow.color"));
    m_shadowXLabel->setText(m_i18n->t("editor", "shape.shadow.offsetX"));
    m_shadowYLabel->setText(m_i18n->t("editor", "shape.shadow.offsetY"));
    m_shadowBlurLabel->setText(m_i18n->t("editor", "shape.shadow.blur"));
    m_shadowBox->setTitle(m_i18n->t("editor", "shape.shadow.title"));
    const int currentIdx = m_kindCombo->currentIndex();
    const int currentData = (currentIdx >= 0) ? m_kindCombo->itemData(currentIdx).toInt() : 0;
    m_kindCombo->clear();
    m_kindCombo->addItem(m_i18n->t("editor", "shape.kind.rect"), static_cast<int>(ShapeKind::Rectangle));
    m_kindCombo->addItem(m_i18n->t("editor", "shape.kind.roundedRect"), static_cast<int>(ShapeKind::RoundedRect));
    m_kindCombo->addItem(m_i18n->t("editor", "shape.kind.ellipse"), static_cast<int>(ShapeKind::Ellipse));
    m_kindCombo->addItem(m_i18n->t("editor", "shape.kind.line"), static_cast<int>(ShapeKind::Line));
    m_kindCombo->addItem(m_i18n->t("editor", "shape.kind.arrowRight"), static_cast<int>(ShapeKind::ArrowRight));
    m_kindCombo->addItem(m_i18n->t("editor", "shape.kind.arrowCurved"), static_cast<int>(ShapeKind::ArrowCurved));
    m_kindCombo->addItem(m_i18n->t("editor", "shape.kind.star"), static_cast<int>(ShapeKind::Star));
    m_kindCombo->addItem(m_i18n->t("editor", "shape.kind.badge"), static_cast<int>(ShapeKind::Badge));
    m_kindCombo->addItem(m_i18n->t("editor", "shape.kind.speechBubble"), static_cast<int>(ShapeKind::SpeechBubble));
    m_kindCombo->addItem(m_i18n->t("editor", "shape.kind.heart"), static_cast<int>(ShapeKind::Heart));
    m_kindCombo->addItem(m_i18n->t("editor", "shape.kind.triangle"), static_cast<int>(ShapeKind::Triangle));
    m_kindCombo->addItem(m_i18n->t("editor", "shape.kind.diamond"), static_cast<int>(ShapeKind::Diamond));
    m_kindCombo->addItem(m_i18n->t("editor", "shape.kind.pentagon"), static_cast<int>(ShapeKind::Pentagon));
    m_kindCombo->addItem(m_i18n->t("editor", "shape.kind.hexagon"), static_cast<int>(ShapeKind::Hexagon));
    m_kindCombo->addItem(m_i18n->t("editor", "shape.kind.cross"), static_cast<int>(ShapeKind::Cross));
    m_kindCombo->addItem(m_i18n->t("editor", "shape.kind.checkmark"), static_cast<int>(ShapeKind::Checkmark));
    m_kindCombo->addItem(m_i18n->t("editor", "shape.kind.cloud"), static_cast<int>(ShapeKind::Cloud));
    m_kindCombo->addItem(m_i18n->t("editor", "shape.kind.explosion"), static_cast<int>(ShapeKind::Explosion));

    for (int i = 0; i < m_kindCombo->count(); ++i) {
        if (m_kindCombo->itemData(i).toInt() == currentData) {
            m_kindCombo->setCurrentIndex(i);
            break;
        }
    }
}

} // namespace cc
