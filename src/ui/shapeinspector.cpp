#include "shapeinspector.h"

// Inclusão dos serviços de internacionalização e histórico de comandos
#include "localization/i18nservice.h"
#include "core/history/CommandStack.h"
#include "core/history/DocumentCommands.h"

// Inclusões de componentes de interface gráfica do Qt
#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

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

    // O raio dos cantos só se aplica a retângulos arredondados (RoundedRect)
    const bool isRounded = (layer->kind == ShapeKind::RoundedRect);
    m_cornerRadiusLabel->setVisible(isRounded);
    m_cornerRadiusSpin->setVisible(isRounded);

    m_loading = false;
}

// Constrói os componentes da interface gráfica do inspetor
void ShapeInspector::buildUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);

    m_groupBox = new QGroupBox(this);
    m_formLayout = new QFormLayout(m_groupBox);

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
    mainLayout->addStretch();
}

// Atualiza todos os textos com base no serviço de internacionalização
void ShapeInspector::retranslateUi()
{
    if (!m_i18n)
        return;

    m_groupBox->setTitle(m_i18n->t("editor", "shape.title"));
    m_fillLabel->setText(m_i18n->t("editor", "shape.fill"));
    m_strokeLabel->setText(m_i18n->t("editor", "shape.stroke"));
    m_strokeWidthLabel->setText(m_i18n->t("editor", "shape.strokeWidth"));
    m_cornerRadiusLabel->setText(m_i18n->t("editor", "shape.cornerRadius"));
}

} // namespace cc
