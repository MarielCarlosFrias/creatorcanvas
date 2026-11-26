#include "smartfilterdialog.h"

#include "localization/i18nservice.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace cc {

SmartFilterDialog::SmartFilterDialog(const QImage& source, AdjustmentType initial,
                                     I18nService* i18n, QWidget* parent)
    : QDialog(parent)
    , m_source(source)
    , m_i18n(i18n)
    , m_buildingUi(true)
{
    setWindowTitle(text("smartFilters.title", QStringLiteral("Filtros Inteligentes")));
    setModal(true);
    setMinimumSize(560, 480);
    resize(720, 600);
    buildUi();

    int initialIndex = 0;
    for (int i = 0; i < m_filterCombo->count(); ++i) {
        if (m_filterCombo->itemData(i).toInt() == static_cast<int>(initial)) {
            initialIndex = i;
            break;
        }
    }
    m_filterCombo->setCurrentIndex(initialIndex);
    if (auto* pages = findChild<QStackedWidget*>())
        pages->setCurrentIndex(initialIndex);
    m_buildingUi = false;
    onFilterChanged(initialIndex);
}

QString SmartFilterDialog::text(const char* key, const QString& fallback) const
{
    return m_i18n ? m_i18n->t("editor", QLatin1String(key)) : fallback;
}

void SmartFilterDialog::buildUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    m_filterCombo = new QComboBox(this);
    m_filterCombo->setObjectName(QStringLiteral("smartFilterType"));
    struct FilterEntry {
        AdjustmentType type;
        const char* key;
        const char* fallback;
    };
    static const FilterEntry entries[] = {
        {AdjustmentType::Blur, "filters.blurGaussian", "Desfocagem Gaussiana"},
        {AdjustmentType::MotionBlur, "filters.blurMotion", "Desfocagem de Movimento"},
        {AdjustmentType::Noise, "filters.noiseAdd", "Adicionar Ruído"},
        {AdjustmentType::DistortTwirl, "filters.distortTwirl", "Torção (Twirl)"},
        {AdjustmentType::Pinch, "filters.distortPinch", "Pinça (Pinch)"},
        {AdjustmentType::Ripple, "filters.distortRipple", "Ondulação (Ripple)"},
    };
    for (const auto& entry : entries) {
        m_filterCombo->addItem(text(entry.key, QString::fromUtf8(entry.fallback)),
                               static_cast<int>(entry.type));
    }
    layout->addWidget(m_filterCombo);

    auto* content = new QHBoxLayout;
    content->setSpacing(10);
    auto* pages = new QStackedWidget(this);
    pages->setMinimumWidth(250);
    content->addWidget(pages);

    auto makeSpin = [this](double minimum, double maximum, double value,
                           int decimals, const QString& suffix,
                           const QString& objectName) {
        auto* spin = new QDoubleSpinBox(this);
        spin->setObjectName(objectName);
        spin->setRange(minimum, maximum);
        spin->setDecimals(decimals);
        spin->setSingleStep(decimals > 1 ? 0.05 : 1.0);
        spin->setValue(value);
        spin->setSuffix(suffix);
        spin->setKeyboardTracking(false);
        connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
                [this](double) { schedulePreviewUpdate(); });
        return spin;
    };
    auto makePage = [this, pages]() {
        auto* page = new QWidget(this);
        auto* form = new QFormLayout(page);
        form->setContentsMargins(4, 8, 4, 8);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        pages->addWidget(page);
        return qobject_cast<QFormLayout*>(page->layout());
    };

    auto* blurForm = makePage();
    m_radius = makeSpin(0.0, 100.0, 5.0, 1, QStringLiteral(" px"),
                        QStringLiteral("smartFilter.blur.radius"));
    blurForm->addRow(text("filters.prompt.radius", QStringLiteral("Raio:")), m_radius);
    blurForm->setRowWrapPolicy(QFormLayout::WrapLongRows);

    auto* motionForm = makePage();
    m_angle = makeSpin(-360.0, 360.0, 0.0, 1, QStringLiteral(" °"),
                       QStringLiteral("smartFilter.motionBlur.angle"));
    m_length = makeSpin(0.0, 200.0, 15.0, 1, QStringLiteral(" px"),
                        QStringLiteral("smartFilter.motionBlur.length"));
    motionForm->addRow(text("filters.prompt.angle", QStringLiteral("Ângulo:")), m_angle);
    motionForm->addRow(text("filters.prompt.length", QStringLiteral("Comprimento:")), m_length);

    auto* noiseForm = makePage();
    m_noiseAmount = makeSpin(0.0, 100.0, 10.0, 1, QStringLiteral(" %"),
                             QStringLiteral("smartFilter.noise.amount"));
    m_monochromeCheck = new QCheckBox(
        text("filters.prompt.monochrome", QStringLiteral("Monocromático")), this);
    m_monochromeCheck->setObjectName(QStringLiteral("smartFilter.noise.monochrome"));
    connect(m_monochromeCheck, &QCheckBox::toggled, this,
            [this](bool) { schedulePreviewUpdate(); });
    noiseForm->addRow(text("filters.prompt.amount", QStringLiteral("Quantidade:")),
                      m_noiseAmount);
    noiseForm->addRow(QString(), m_monochromeCheck);

    auto* twirlForm = makePage();
    m_twirlAmount = makeSpin(-1.0, 1.0, 0.5, 2, QString(),
                             QStringLiteral("smartFilter.twirl.amount"));
    twirlForm->addRow(text("filters.prompt.amount", QStringLiteral("Quantidade:")),
                      m_twirlAmount);

    auto* pinchForm = makePage();
    m_pinchAmount = makeSpin(-0.9, 0.9, 0.5, 2, QString(),
                             QStringLiteral("smartFilter.pinch.amount"));
    pinchForm->addRow(text("filters.prompt.amount", QStringLiteral("Quantidade:")),
                      m_pinchAmount);

    auto* rippleForm = makePage();
    m_rippleAmount = makeSpin(-1.0, 1.0, 0.5, 2, QString(),
                              QStringLiteral("smartFilter.ripple.amount"));
    m_wavelength = makeSpin(0.0, 1000.0, 0.0, 1, QStringLiteral(" px"),
                            QStringLiteral("smartFilter.ripple.wavelength"));
    m_wavelength->setSpecialValueText(
        text("smartFilters.wavelengthAuto", QStringLiteral("Automático")));
    m_wavelengthLabel = new QLabel(
        text("smartFilters.wavelength", QStringLiteral("Comprimento de onda:")), this);
    rippleForm->addRow(text("filters.prompt.amount", QStringLiteral("Quantidade:")),
                       m_rippleAmount);
    rippleForm->addRow(m_wavelengthLabel, m_wavelength);

    auto* rightColumn = new QVBoxLayout;
    m_previewLabel = new QLabel(this);
    m_previewLabel->setObjectName(QStringLiteral("smartFilterPreview"));
    m_previewLabel->setMinimumSize(260, 220);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_previewLabel->setStyleSheet(QStringLiteral(
        "background-color: #202227; border: 1px solid #2b303c; border-radius: 6px;"));
    rightColumn->addWidget(m_previewLabel, 1);
    content->addLayout(rightColumn, 1);
    layout->addLayout(content, 1);

    m_nonDestructiveCheck = new QCheckBox(
        text("smartFilters.nonDestructive",
             QStringLiteral("Não destrutivo (Smart Filter / camada de ajuste)")),
        this);
    m_nonDestructiveCheck->setObjectName(QStringLiteral("smartFilterNonDestructive"));
    m_nonDestructiveCheck->setChecked(true);
    m_nonDestructiveCheck->setToolTip(text(
        "smartFilters.nonDestructiveTip",
        QStringLiteral("Mantém o asset original e permite editar tipo e parâmetros depois. "
                       "Desmarque para cozer o filtro num novo asset.")));
    layout->addWidget(m_nonDestructiveCheck);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                         this);
    buttons->button(QDialogButtonBox::Ok)->setText(
        text("smartFilters.apply", QStringLiteral("Aplicar")));
    buttons->button(QDialogButtonBox::Cancel)->setText(
        text("smartFilters.cancel", QStringLiteral("Cancelar")));
    layout->addWidget(buttons);

    connect(m_filterCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this, pages](int index) {
                pages->setCurrentIndex(index);
                onFilterChanged(index);
            });
    connect(buttons, &QDialogButtonBox::accepted, this, &SmartFilterDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

AdjustmentParams SmartFilterDialog::currentParams(AdjustmentType type) const
{
    const auto value = [](const QDoubleSpinBox* spin, double fallback) {
        return spin ? spin->value() : fallback;
    };
    switch (type) {
    case AdjustmentType::Blur:
        return {{QStringLiteral("radius"), value(m_radius, 5.0)}};
    case AdjustmentType::MotionBlur:
        return {{QStringLiteral("angle"), value(m_angle, 0.0)},
                {QStringLiteral("length"), value(m_length, 15.0)}};
    case AdjustmentType::Noise:
        return {{QStringLiteral("amount"), value(m_noiseAmount, 10.0)},
                {QStringLiteral("monochrome"),
                 m_monochromeCheck && m_monochromeCheck->isChecked() ? 1.0 : 0.0}};
    case AdjustmentType::DistortTwirl:
        return {{QStringLiteral("amount"), value(m_twirlAmount, 0.5)}};
    case AdjustmentType::Pinch:
        return {{QStringLiteral("amount"), value(m_pinchAmount, 0.5)}};
    case AdjustmentType::Ripple:
        return {{QStringLiteral("amount"), value(m_rippleAmount, 0.5)},
                {QStringLiteral("wavelength"), value(m_wavelength, 0.0)}};
    default:
        return {};
    }
}

SmartFilterRequest SmartFilterDialog::request() const
{
    SmartFilterRequest result;
    if (m_filterCombo && m_filterCombo->currentIndex() >= 0) {
        result.type = static_cast<AdjustmentType>(m_filterCombo->currentData().toInt());
    }
    result.params = currentParams(result.type);
    result.nonDestructive = !m_nonDestructiveCheck || m_nonDestructiveCheck->isChecked();
    result.valid = !m_source.isNull() && isSmartFilterType(result.type);
    return result;
}

void SmartFilterDialog::onFilterChanged(int index)
{
    Q_UNUSED(index);
    if (!m_buildingUi)
        updatePreview();
}

void SmartFilterDialog::schedulePreviewUpdate()
{
    if (m_buildingUi)
        return;
    if (!m_previewTimer) {
        m_previewTimer = new QTimer(this);
        m_previewTimer->setSingleShot(true);
        m_previewTimer->setInterval(60);
        connect(m_previewTimer, &QTimer::timeout, this, &SmartFilterDialog::updatePreview);
    }
    m_previewTimer->start();
}

void SmartFilterDialog::updatePreview()
{
    if (m_buildingUi || !m_previewLabel)
        return;
    if (m_source.isNull()) {
        m_preview = {};
        m_previewLabel->setText(text("smartFilters.noImage", QStringLiteral("Sem imagem")));
        return;
    }

    QImage small = m_source;
    constexpr int maxPreviewSide = 480;
    if (std::max(small.width(), small.height()) > maxPreviewSide) {
        small = small.scaled(QSize(maxPreviewSide, maxPreviewSide), Qt::KeepAspectRatio,
                             Qt::SmoothTransformation);
    }
    const SmartFilterRequest current = request();
    m_preview = applyAdjustmentParameters(small, current.type, current.params);
    m_previewLabel->setText(QString());
    showPreviewPixmap();
}

void SmartFilterDialog::showPreviewPixmap()
{
    if (!m_previewLabel || m_preview.isNull())
        return;
    const QSize target = m_previewLabel->size().expandedTo(QSize(1, 1));
    m_previewLabel->setPixmap(QPixmap::fromImage(m_preview).scaled(
        target, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void SmartFilterDialog::accept()
{
    // Recolhe também uma alteração feita imediatamente antes de clicar em
    // Aplicar, sem depender do timer da prévia.
    updatePreview();
    QDialog::accept();
}

void SmartFilterDialog::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    showPreviewPixmap();
}

} // namespace cc
