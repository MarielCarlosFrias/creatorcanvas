#include "adjustmentsdialog.h"

#include "curveeditor.h"
#include "histogramview.h"
#include "core/image/ImageHistogram.h"
#include "designtokens.h"
#include "localization/i18nservice.h"

#include <QCheckBox>
#include <QComboBox>
#include <QTimer>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QColorDialog>
#include <QGuiApplication>
#include <QListWidgetItem>
#include <QResizeEvent>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QSlider>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>

#include <utility>

namespace cc {

namespace {

/// Tipos expostos no diálogo, por esta ordem.
const AdjustmentType kTypes[] = {
    AdjustmentType::BrightnessContrast,
    AdjustmentType::HueSaturation,
    AdjustmentType::ShadowsHighlights,
    AdjustmentType::Vignette,
    AdjustmentType::Invert,
    AdjustmentType::Posterize,
    AdjustmentType::Threshold,
    AdjustmentType::Levels,
    AdjustmentType::Curves,
    AdjustmentType::ChannelMixer,
    AdjustmentType::GradientMap,
};

/// Rótulos dos sliders simples, por tipo (mesma ordem de kTypes): chave i18n
/// + texto de reserva legível (usado quando não há I18nService, como em
/// testes). `nullptr` = o tipo usa página dedicada, não a de dois sliders.
struct SliderLabels { const char* keyA; const char* fallbackA;
                      const char* keyB; const char* fallbackB; };
const SliderLabels kSliderLabels[] = {
    {"adjustments.brightness", "Brilho", "adjustments.contrast", "Contraste"},
    {"adjustments.hue", "Matiz", "adjustments.saturation", "Saturação"},
    {"adjustments.shadows", "Sombras", "adjustments.highlights", "Realces"},
    {nullptr, nullptr, nullptr, nullptr},                    // Vinheta: página própria
    {nullptr, nullptr, nullptr, nullptr}, {nullptr, nullptr, nullptr, nullptr},
    {nullptr, nullptr, nullptr, nullptr}, {nullptr, nullptr, nullptr, nullptr},
    {nullptr, nullptr, nullptr, nullptr}, {nullptr, nullptr, nullptr, nullptr},
    {nullptr, nullptr, nullptr, nullptr},
};

bool typeHasParams(AdjustmentType type)
{
    return type != AdjustmentType::Invert && type != AdjustmentType::Posterize
        && type != AdjustmentType::Threshold;
}

} // namespace

AdjustmentsDialog::AdjustmentsDialog(const QImage& source, AdjustmentType initial,
                                     I18nService* i18n, QWidget* parent,
                                     bool advancedMode)
    : QDialog(parent)
    , m_source(source)
    , m_i18n(i18n)
    , m_advancedMode(advancedMode)
    , m_buildingUi(true)
{
    m_request.type = initial;
    setWindowTitle(tr("title", QStringLiteral("Ajustes de Imagem - CreatorCanvas")));
    setMinimumSize(640, 480);
    setSizeGripEnabled(true);
    const QScreen* screen = parentWidget() ? parentWidget()->screen()
                                           : QGuiApplication::primaryScreen();
    const QRect available = screen ? screen->availableGeometry()
                                   : QRect(0, 0, 1280, 800);
    resize(qBound(640, qMin(820, available.width() - 32), available.width()),
           qBound(480, qMin(640, available.height() - 48), available.height()));

    buildUi();
    m_buildingUi = false;
    for (int i = 0; i < static_cast<int>(sizeof(kTypes) / sizeof(kTypes[0])); ++i) {
        if (kTypes[i] == initial) {
            m_typeList->setCurrentRow(i);
            break;
        }
    }
    updatePreview();
}

QString AdjustmentsDialog::tr(const char* key, const QString& fallback) const
{
    return m_i18n ? m_i18n->t("editor", QStringLiteral("adjustments.") + QLatin1String(key))
                  : fallback;
}

void AdjustmentsDialog::buildUi()
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    // --- Coluna esquerda: tipos de ajuste ---
    m_typeList = new QListWidget(this);
    m_typeList->setMaximumWidth(190);
    m_typeList->setSpacing(2);
    struct Entry { AdjustmentType type; const char* key; const char* fallback; };
    static const Entry entries[] = {
        {AdjustmentType::BrightnessContrast, "brightnessContrast", "Brilho / Contraste"},
        {AdjustmentType::HueSaturation, "hueSaturation", "Matiz / Saturação"},
        {AdjustmentType::ShadowsHighlights, "shadowsHighlights", "Sombras / Realces"},
        {AdjustmentType::Vignette, "vignette", "Vinheta"},
        {AdjustmentType::Invert, "invert", "Inverter Cores"},
        {AdjustmentType::Posterize, "posterize", "Posterizar"},
        {AdjustmentType::Threshold, "threshold", "Threshold (P&B)"},
        {AdjustmentType::Levels, "levels", "Níveis (Levels)"},
        {AdjustmentType::Curves, "curves", "Curvas (Curves)"},
        {AdjustmentType::ChannelMixer, "channelMixer", "Misturador de Canais"},
        {AdjustmentType::GradientMap, "gradientMap", "Mapa de Gradiente"},
    };
    for (const auto& e : entries) {
        auto* item = new QListWidgetItem(tr(e.key, QString::fromUtf8(e.fallback)));
        item->setData(Qt::UserRole, static_cast<int>(e.type));
        m_typeList->addItem(item);
    }
    layout->addWidget(m_typeList);

    // --- Coluna direita: páginas + prévia ---
    auto* rightColumn = new QVBoxLayout;
    rightColumn->setSpacing(8);

    // O label da prévia é criado ANTES das páginas: construir uma página
    // pode emitir valueChanged (ex.: setValue(255)), que chamaria
    // updatePreview() sobre um label ainda nulo.
    m_previewLabel = new QLabel(this);
    m_previewLabel->setMinimumHeight(150);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setStyleSheet(QStringLiteral(
        "background-color: #202227; border: 1px solid #2b303c; border-radius: 6px;"));

    m_pages = new QStackedWidget(this);
    // Páginas na mesma ordem da lista de tipos (kTypes): sliders para os
    // tipos com parâmetros simples, páginas dedicadas para o resto.
    for (int i = 0; i < static_cast<int>(sizeof(kTypes) / sizeof(kTypes[0])); ++i) {
        const AdjustmentType type = kTypes[i];
        const SliderLabels labels = kSliderLabels[i];
        if (labels.keyA && labels.keyB) {
            QSlider*& outA = (i == 0) ? m_bcSliderA : (i == 1) ? m_hsSliderA : m_shadowSlider;
            QSlider*& outB = (i == 0) ? m_bcSliderB : (i == 1) ? m_hsSliderB : m_highlightSlider;
            m_pages->addWidget(buildSlidersPage(
                outA, outB,
                m_i18n ? m_i18n->t("editor", QString::fromLatin1(labels.keyA))
                       : QString::fromUtf8(labels.fallbackA),
                m_i18n ? m_i18n->t("editor", QString::fromLatin1(labels.keyB))
                       : QString::fromUtf8(labels.fallbackB)));
            continue;
        }
        switch (type) {
        case AdjustmentType::Invert:
        case AdjustmentType::Posterize:
        case AdjustmentType::Threshold:
            m_pages->addWidget(buildSimplePage());
            break;
        case AdjustmentType::Vignette:
            m_pages->addWidget(buildVignettePage());
            break;
        case AdjustmentType::Levels:
            m_pages->addWidget(buildLevelsPage());
            break;
        case AdjustmentType::Curves:
            m_pages->addWidget(buildCurvesPage());
            break;
        case AdjustmentType::ChannelMixer:
            m_pages->addWidget(buildChannelMixerPage());
            break;
        case AdjustmentType::GradientMap:
            m_pages->addWidget(buildGradientMapPage());
            break;
        default:
            m_pages->addWidget(buildSimplePage());
            break;
        }
    }
    rightColumn->addWidget(m_pages, 1);

    // Histograma: ferramenta profissional, só em modo avançado.
    if (m_advancedMode) {
        m_histogramLabel = new QLabel(this);
        m_histogramLabel->setStyleSheet(QStringLiteral("color: #9aa3b2; font-size: 11px;"));
        rightColumn->addWidget(m_histogramLabel);
        m_histogram = new HistogramView(this);
        m_histogram->setMinimumHeight(90);
        rightColumn->addWidget(m_histogram);
    }

    rightColumn->addWidget(m_previewLabel, 1);

    m_nonDestructiveCheck = new QCheckBox(
        tr("nonDestructive", QStringLiteral("Não-destrutivo (camada de ajuste)")), this);
    m_nonDestructiveCheck->setChecked(true);
    m_nonDestructiveCheck->setToolTip(tr(
        "nonDestructiveTip",
        QStringLiteral("Cria uma camada de ajuste ligada a esta camada de imagem. "
                       "Os parâmetros ficam editáveis e o undo recupera o original.")));
    rightColumn->addWidget(m_nonDestructiveCheck);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch();
    auto* cancelBtn = new QPushButton(QStringLiteral("Cancelar"), this);
    auto* applyBtn = new QPushButton(QStringLiteral("✓ Aplicar"), this);
    applyBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: %1; color: white; font-weight: bold; padding: 6px 18px; border-radius: 4px; }"
        "QPushButton:hover { background-color: %2; }")
        .arg(Design::Success().name(), Design::SuccessHover().name()));
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(applyBtn, &QPushButton::clicked, this, &QDialog::accept);
    buttons->addWidget(cancelBtn);
    buttons->addWidget(applyBtn);
    rightColumn->addLayout(buttons);

    layout->addLayout(rightColumn, 1);

    connect(m_typeList, &QListWidget::currentRowChanged, this, &AdjustmentsDialog::onTypeChanged);
}

QWidget* AdjustmentsDialog::buildSlidersPage(QSlider*& outA, QSlider*& outB,
                                            const QString& labelA, const QString& labelB)
{
    auto* page = new QWidget(this);
    auto* grid = new QGridLayout(page);
    grid->setContentsMargins(4, 8, 4, 8);

    auto makeSlider = [this](int lo, int hi, int value, const QString& labelText) {
        auto* label = new QLabel(labelText, this);
        auto* slider = new QSlider(Qt::Horizontal, this);
        slider->setRange(lo, hi);
        slider->setValue(value);
        auto* valueLabel = new QLabel(QString::number(value), this);
        valueLabel->setMinimumWidth(44);
        connect(slider, &QSlider::valueChanged, this, [this, valueLabel](int v) {
            valueLabel->setText(QString::number(v));
            onParamChanged();
        });
        return std::make_tuple(label, slider, valueLabel);
    };

    auto [labelWidgetA, sliderA, valueA] = makeSlider(-100, 100, 0, labelA);
    auto [labelWidgetB, sliderB, valueB] = makeSlider(-100, 100, 0, labelB);
    outA = sliderA;
    outB = sliderB;
    grid->addWidget(labelWidgetA, 0, 0);
    grid->addWidget(sliderA, 0, 1);
    grid->addWidget(valueA, 0, 2);
    grid->addWidget(labelWidgetB, 1, 0);
    grid->addWidget(sliderB, 1, 1);
    grid->addWidget(valueB, 1, 2);
    grid->setRowStretch(2, 1);
    return page;
}

QWidget* AdjustmentsDialog::buildSimplePage()
{
    auto* page = new QWidget(this);
    auto* v = new QVBoxLayout(page);
    auto* hint = new QLabel(tr("noParams", QStringLiteral(
        "Este ajuste não tem parâmetros — aplica-se directamente.")), this);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: #888;"));
    v->addWidget(hint);
    v->addStretch(1);
    return page;
}

QWidget* AdjustmentsDialog::buildLevelsPage()
{
    auto* page = new QWidget(this);
    auto* grid = new QGridLayout(page);
    grid->setContentsMargins(4, 8, 4, 8);

    m_channelCombo = new QComboBox(this);
    m_channelCombo->addItem(QStringLiteral("RGB (Mestre)"));
    m_channelCombo->addItem(QStringLiteral("Vermelho"));
    m_channelCombo->addItem(QStringLiteral("Verde"));
    m_channelCombo->addItem(QStringLiteral("Azul"));
    grid->addWidget(new QLabel(QStringLiteral("Canal:"), this), 0, 0);
    grid->addWidget(m_channelCombo, 0, 1);

    m_inBlack = new QSlider(Qt::Horizontal, this);
    m_inWhite = new QSlider(Qt::Horizontal, this);
    m_outBlack = new QSlider(Qt::Horizontal, this);
    m_outWhite = new QSlider(Qt::Horizontal, this);
    for (QSlider* s : {m_inBlack, m_inWhite, m_outBlack, m_outWhite}) {
        s->setRange(0, 255);
        connect(s, &QSlider::valueChanged, this, &AdjustmentsDialog::onParamChanged);
    }
    m_inBlack->setValue(0);
    m_inWhite->setValue(255);
    m_outBlack->setValue(0);
    m_outWhite->setValue(255);

    m_gamma = new QDoubleSpinBox(this);
    m_gamma->setRange(0.1, 9.99);
    m_gamma->setSingleStep(0.1);
    m_gamma->setValue(1.0);
    connect(m_gamma, &QDoubleSpinBox::valueChanged, this,
            &AdjustmentsDialog::onParamChanged);

    grid->addWidget(new QLabel(QStringLiteral("Preto entrada:"), this), 1, 0);
    grid->addWidget(m_inBlack, 1, 1);
    grid->addWidget(new QLabel(QStringLiteral("Branco entrada:"), this), 2, 0);
    grid->addWidget(m_inWhite, 2, 1);
    grid->addWidget(new QLabel(QStringLiteral("Gama (meios):"), this), 3, 0);
    grid->addWidget(m_gamma, 3, 1);
    grid->addWidget(new QLabel(QStringLiteral("Preto saída:"), this), 4, 0);
    grid->addWidget(m_outBlack, 4, 1);
    grid->addWidget(new QLabel(QStringLiteral("Branco saída:"), this), 5, 0);
    grid->addWidget(m_outWhite, 5, 1);
    grid->setRowStretch(6, 1);

    connect(m_channelCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &AdjustmentsDialog::onChannelChanged);
    return page;
}

QWidget* AdjustmentsDialog::buildCurvesPage()
{
    auto* page = new QWidget(this);
    auto* v = new QVBoxLayout(page);

    m_channelCombo = new QComboBox(this);
    m_channelCombo->addItem(QStringLiteral("RGB (Mestre)"));
    m_channelCombo->addItem(QStringLiteral("Vermelho"));
    m_channelCombo->addItem(QStringLiteral("Verde"));
    m_channelCombo->addItem(QStringLiteral("Azul"));
    v->addWidget(m_channelCombo);

    m_curve = new CurveEditor(this);
    m_curveMaster = {CurvePoint{0.0f, 0.0f}, CurvePoint{1.0f, 1.0f}};
    m_curveR = m_curveMaster;
    m_curveG = m_curveMaster;
    m_curveB = m_curveMaster;
    connect(m_curve, &CurveEditor::pointsChanged, this, &AdjustmentsDialog::onParamChanged);
    v->addWidget(m_curve, 1);

    connect(m_channelCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &AdjustmentsDialog::onChannelChanged);
    return page;
}

QWidget* AdjustmentsDialog::buildVignettePage()
{
    auto* page = new QWidget(this);
    auto* grid = new QGridLayout(page);
    grid->setContentsMargins(4, 8, 4, 8);

    auto* hint = new QLabel(tr("vignetteHint", QStringLiteral(
        "Positivo escurece as bordas; negativo clareia.")), this);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: #888;"));
    grid->addWidget(hint, 0, 0, 1, 2);

    m_vignetteSlider = new QSlider(Qt::Horizontal, this);
    m_vignetteSlider->setRange(-100, 100);
    m_vignetteSlider->setValue(0);
    auto* value = new QLabel(QStringLiteral("0"), this);
    value->setMinimumWidth(44);
    connect(m_vignetteSlider, &QSlider::valueChanged, this, [this, value](int v) {
        value->setText(QString::number(v));
        onParamChanged();
    });
    grid->addWidget(new QLabel(tr("vignette", QStringLiteral("Vinheta")), this), 1, 0);
    grid->addWidget(m_vignetteSlider, 1, 1);
    grid->addWidget(value, 1, 2);
    grid->setRowStretch(2, 1);
    return page;
}

QWidget* AdjustmentsDialog::buildChannelMixerPage()
{
    auto* page = new QWidget(this);
    auto* grid = new QGridLayout(page);
    grid->setContentsMargins(4, 8, 4, 8);

    auto* hint = new QLabel(tr("channelMixerHint", QStringLiteral(
        "Ganho por canal de saída (100% = original).")), this);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: #888;"));
    grid->addWidget(hint, 0, 0, 1, 3);

    auto makeGain = [this, grid](const QString& label, QSlider*& out, int row) {
        auto* slider = new QSlider(Qt::Horizontal, this);
        slider->setRange(0, 200);
        slider->setValue(100);
        auto* value = new QLabel(QStringLiteral("100%"), this);
        value->setMinimumWidth(44);
        connect(slider, &QSlider::valueChanged, this, [this, value](int v) {
            value->setText(QStringLiteral("%1%").arg(v));
            onParamChanged();
        });
        grid->addWidget(new QLabel(label, this), row, 0);
        grid->addWidget(slider, row, 1);
        grid->addWidget(value, row, 2);
        out = slider;
    };
    makeGain(QStringLiteral("R"), m_cmRed, 1);
    makeGain(QStringLiteral("G"), m_cmGreen, 2);
    makeGain(QStringLiteral("B"), m_cmBlue, 3);
    grid->setRowStretch(4, 1);
    return page;
}

QWidget* AdjustmentsDialog::buildGradientMapPage()
{
    auto* page = new QWidget(this);
    auto* grid = new QGridLayout(page);
    grid->setContentsMargins(4, 8, 4, 8);

    m_color1Button = new QPushButton(QStringLiteral("Sombra"), this);
    m_color2Button = new QPushButton(QStringLiteral("Realce"), this);
    auto applySwatch = [](QPushButton* b, const QColor& c) {
        b->setStyleSheet(QStringLiteral("padding: 6px 12px; border-radius: 4px;")
                         + QStringLiteral("background-color: %1; color: %2;")
                             .arg(c.name(),
                                  c.lightness() > 128 ? QStringLiteral("#111") : QStringLiteral("#eee")));
    };
    applySwatch(m_color1Button, m_color1);
    applySwatch(m_color2Button, m_color2);
    connect(m_color1Button, &QPushButton::clicked, this, [this] {
        const QColor c = QColorDialog::getColor(m_color1, this, QStringLiteral("Cor de sombra"));
        if (c.isValid()) { m_color1 = c; onParamChanged(); }
    });
    connect(m_color2Button, &QPushButton::clicked, this, [this] {
        const QColor c = QColorDialog::getColor(m_color2, this, QStringLiteral("Cor de realce"));
        if (c.isValid()) { m_color2 = c; onParamChanged(); }
    });
    m_reverseCheck = new QCheckBox(QStringLiteral("Inverter mapa"), this);
    connect(m_reverseCheck, &QCheckBox::toggled, this, &AdjustmentsDialog::onParamChanged);

    grid->addWidget(m_color1Button, 0, 0);
    grid->addWidget(m_color2Button, 0, 1);
    grid->addWidget(m_reverseCheck, 1, 0, 1, 2);
    grid->setRowStretch(2, 1);
    return page;
}

QString AdjustmentsDialog::channelPrefix() const
{
    switch (m_channelCombo ? m_channelCombo->currentIndex() : 0) {
    case 1: return QStringLiteral(".r");
    case 2: return QStringLiteral(".g");
    case 3: return QStringLiteral(".b");
    default: return QString();
    }
}

LevelsChannel& AdjustmentsDialog::activeLevels()
{
    switch (m_channelCombo ? m_channelCombo->currentIndex() : 0) {
    case 1: return m_levelsR;
    case 2: return m_levelsG;
    case 3: return m_levelsB;
    default: return m_levelsMaster;
    }
}

std::vector<CurvePoint>& AdjustmentsDialog::activeCurve()
{
    switch (m_channelCombo ? m_channelCombo->currentIndex() : 0) {
    case 1: return m_curveR;
    case 2: return m_curveG;
    case 3: return m_curveB;
    default: return m_curveMaster;
    }
}

void AdjustmentsDialog::updateHistogram()
{
    if (!m_histogram || !m_histogramLabel)
        return;
    if (m_preview.isNull())
        return;

    const ImageHistogram hist = ::cc::computeHistogram(m_preview);
    m_histogram->setHistogram(hist);

    // Nos níveis, os marcadores mostram onde os blacks/whites caem.
    int inBlack = -1, inWhite = -1, outBlack = -1, outWhite = -1;
    if (m_request.type == AdjustmentType::Levels) {
        inBlack = static_cast<int>(m_levelsMaster.inBlack * 255);
        inWhite = static_cast<int>(m_levelsMaster.inWhite * 255);
        outBlack = static_cast<int>(m_levelsMaster.outBlack * 255);
        outWhite = static_cast<int>(m_levelsMaster.outWhite * 255);
    }
    m_histogram->setLevelMarkers(inBlack, inWhite, outBlack, outWhite);
    m_histogramLabel->setText(tr("histogram", QStringLiteral("Histograma")));
}

void AdjustmentsDialog::onTypeChanged(int row)
{
    if (row < 0 || row >= m_pages->count())
        return;
    m_pages->setCurrentIndex(row);
    m_request.type = static_cast<AdjustmentType>(m_typeList->item(row)->data(Qt::UserRole).toInt());
    m_nonDestructiveCheck->setEnabled(typeHasParams(m_request.type));
    updatePreview();
}

void AdjustmentsDialog::onChannelChanged(int)
{
    const bool levelsPage = m_request.type == AdjustmentType::Levels;
    if (levelsPage) {
        // Sliders passam a reflectir o canal activo.
        const LevelsChannel& ch = activeLevels();
        const QSignalBlocker b1(m_inBlack); const QSignalBlocker b2(m_inWhite);
        const QSignalBlocker b3(m_outBlack); const QSignalBlocker b4(m_outWhite);
        const QSignalBlocker b5(m_gamma);
        m_inBlack->setValue(static_cast<int>(ch.inBlack * 255));
        m_inWhite->setValue(static_cast<int>(ch.inWhite * 255));
        m_outBlack->setValue(static_cast<int>(ch.outBlack * 255));
        m_outWhite->setValue(static_cast<int>(ch.outWhite * 255));
        m_gamma->setValue(ch.gamma);
    } else if (m_request.type == AdjustmentType::Curves && m_curve) {
        m_curve->setPoints(activeCurve());
    }
}

void AdjustmentsDialog::onParamChanged()
{
    //Durante a construção os controlos ainda não existem todos.
    if (m_buildingUi)
        return;

    if (m_request.type == AdjustmentType::Levels) {
        LevelsChannel& ch = activeLevels();
        ch.inBlack = static_cast<float>(m_inBlack->value()) / 255.0f;
        ch.inWhite = static_cast<float>(m_inWhite->value()) / 255.0f;
        ch.gamma = static_cast<float>(m_gamma->value());
        ch.outBlack = static_cast<float>(m_outBlack->value()) / 255.0f;
        ch.outWhite = static_cast<float>(m_outWhite->value()) / 255.0f;
    } else if (m_request.type == AdjustmentType::Curves && m_curve) {
        activeCurve() = m_curve->points();
    }
    schedulePreviewUpdate();
}

void AdjustmentsDialog::schedulePreviewUpdate()
{
    if (!m_previewTimer) {
        m_previewTimer = new QTimer(this);
        m_previewTimer->setSingleShot(true);
        m_previewTimer->setInterval(60);
        connect(m_previewTimer, &QTimer::timeout, this, &AdjustmentsDialog::updatePreview);
    }
    m_previewTimer->start();
}

void AdjustmentsDialog::collectParams()
{
    switch (m_request.type) {
    case AdjustmentType::BrightnessContrast:
        m_request.params = {
            {QStringLiteral("brightness"), m_bcSliderA ? m_bcSliderA->value() : 0.0},
            {QStringLiteral("contrast"), m_bcSliderB ? m_bcSliderB->value() : 0.0},
        };
        break;
    case AdjustmentType::HueSaturation:
        m_request.params = {
            {QStringLiteral("hue"), m_hsSliderA ? m_hsSliderA->value() : 0.0},
            {QStringLiteral("saturation"), m_hsSliderB ? m_hsSliderB->value() : 0.0},
        };
        break;
    case AdjustmentType::ShadowsHighlights:
        m_request.params = {
            {QStringLiteral("shadows"), m_shadowSlider ? m_shadowSlider->value() : 0.0},
            {QStringLiteral("highlights"), m_highlightSlider ? m_highlightSlider->value() : 0.0},
        };
        break;
    case AdjustmentType::Vignette:
        m_request.params = {
            {QStringLiteral("vignette"), m_vignetteSlider ? m_vignetteSlider->value() : 0.0},
        };
        break;
    case AdjustmentType::ChannelMixer:
        m_request.params = {
            {QStringLiteral("redGain"), m_cmRed ? m_cmRed->value() / 100.0 : 1.0},
            {QStringLiteral("greenGain"), m_cmGreen ? m_cmGreen->value() / 100.0 : 1.0},
            {QStringLiteral("blueGain"), m_cmBlue ? m_cmBlue->value() / 100.0 : 1.0},
        };
        break;
    case AdjustmentType::Levels:
        m_request.params = encodeLevels(m_levelsMaster, m_levelsR, m_levelsG, m_levelsB);
        break;
    case AdjustmentType::Curves:
        m_request.params = encodeCurves(m_curveMaster, m_curveR, m_curveG, m_curveB);
        break;
    case AdjustmentType::GradientMap: {
        const auto encodeColor = [](const QString& prefix, const QColor& c) {
            return AdjustmentParams{
                {prefix + QStringLiteral(".r"), c.redF()},
                {prefix + QStringLiteral(".g"), c.greenF()},
                {prefix + QStringLiteral(".b"), c.blueF()},
            };
        };
        m_request.params = encodeColor(QStringLiteral("color1"), m_color1);
        for (auto&& kv : encodeColor(QStringLiteral("color2"), m_color2))
            m_request.params.emplace_back(kv.first, kv.second);
        m_request.params.emplace_back(QStringLiteral("reverse"),
                                      m_reverseCheck && m_reverseCheck->isChecked() ? 1.0 : 0.0);
        break;
    }
    default:
        m_request.params.clear();
        break;
    }
    m_request.nonDestructive = m_nonDestructiveCheck
        && m_nonDestructiveCheck->isChecked() && typeHasParams(m_request.type);
    m_request.valid = !m_source.isNull();
}

void AdjustmentsDialog::updatePreview()
{
    // O diálogo ainda está a ser construído (ou nada para mostrar).
    if (m_source.isNull() || !m_previewLabel || !m_pages)
        return;

    collectParams();

    // Prévia em escala reduzida: ajustes são por-pixel e a imagem pode
    // ter megapixels; 480px do lado maior chega para julgar o resultado.
    QImage small = m_source;
    if (std::max(small.width(), small.height()) > 480) {
        small = small.scaled(small.width() > small.height()
                                 ? QSize(480, std::max(1, small.height() * 480 / small.width()))
                                 : QSize(std::max(1, small.width() * 480 / small.height()), 480),
                             Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    m_preview = applyAdjustmentParameters(small, m_request.type, m_request.params);
    updateHistogram();
    m_previewLabel->setPixmap(QPixmap::fromImage(m_preview)
                                  .scaled(m_previewLabel->size(), Qt::KeepAspectRatio,
                                          Qt::SmoothTransformation));
}

void AdjustmentsDialog::accept()
{
    // Garante que o pedido reflecte o estado final dos controlos, mesmo
    // que o utilizador não tenha tocado em nada desde a abertura.
    collectParams();
    QDialog::accept();
}

void AdjustmentsDialog::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    // A prévia é um pixmap escalado: redesenhar no resize evita mosaico.
    if (!m_preview.isNull()) {
        m_previewLabel->setPixmap(QPixmap::fromImage(m_preview)
                                      .scaled(m_previewLabel->size(), Qt::KeepAspectRatio,
                                              Qt::SmoothTransformation));
    }
}

} // namespace cc
